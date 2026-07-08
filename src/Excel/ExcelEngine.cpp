#include "ExcelEngine.h"
#include "xlsxdocument.h"
#include "xlsxreadsax.h"
#include "xlsxzipreader_p.h"
#include "xlsxzipwriter_p.h"
#include "xlsxworkbook.h"
#include "xlsxworksheet.h"

#include <QFile>
#include <QDateTime>
#include <QMutex>
#include <QAtomicInt>
#include <QtConcurrent>
#include <QThreadPool>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

QXLSX_USE_NAMESPACE

ExcelEngine::ExcelEngine(QObject *parent)
    : QObject(parent)
{
}

ExcelEngine::~ExcelEngine()
{
}

static inline QString normalizeProvinceFast(const QString &province) {
    QString s = province;
    s.reserve(province.size());
    static const QStringList suffixes = {
        QStringLiteral("省"), QStringLiteral("市"),
        QStringLiteral("自治区"), QStringLiteral("特别行政区")
    };
    for (const QString &sf : suffixes) {
        if (s.endsWith(sf)) {
            s.chop(sf.size());
            break;
        }
    }
    return s.trimmed();
}

int ExcelEngine::countRowsFast(const QString &filePath)
{
    if (!QFile::exists(filePath))
        return 0;

    ZipReader zip(filePath);
    if (!zip.exists())
        return 0;

    QByteArray sheetXml = zip.fileData("xl/worksheets/sheet1.xml");
    if (sheetXml.isEmpty())
        return 0;

    QXmlStreamReader rd(sheetXml);
    int rowCount = 0;
    bool inSheetData = false;

    while (!rd.atEnd()) {
        rd.readNext();
        if (rd.isStartElement()) {
            if (rd.name() == QLatin1String("sheetData")) {
                inSheetData = true;
            } else if (inSheetData && rd.name() == QLatin1String("row")) {
                ++rowCount;
            }
        } else if (rd.isEndElement()) {
            if (rd.name() == QLatin1String("sheetData")) {
                break;
            }
        }
    }

    return qMax(0, rowCount - 1);
}

QStringList ExcelEngine::getHeaders(const QString &filePath)
{
    QStringList headers;

    if (!QFile::exists(filePath)) {
        emit errorOccurred(tr("文件不存在: %1").arg(filePath));
        return headers;
    }

    ZipReader zip(filePath);
    if (!zip.exists()) {
        emit errorOccurred(tr("无法打开文件: %1").arg(filePath));
        return headers;
    }

    QStringList sharedStrings = load_shared_strings_all(zip);
    QByteArray sheetXml = zip.fileData("xl/worksheets/sheet1.xml");

    sax_options opt;
    opt.resolve_shared_strings = true;
    opt.stop_on_empty_sheetdata = true;

    int maxCol = 0;
    QMap<int, QString> firstRowCells;

    auto onCell = [&](const sax_cell& cell) -> bool {
        if (cell.row > 1)
            return false;
        if (cell.col > maxCol)
            maxCol = cell.col;
        firstRowCells[cell.col] = cell.value.toString().trimmed();
        return true;
    };

    read_sheet_xml_sax(sheetXml, opt, &sharedStrings, onCell);

    for (int c = 1; c <= maxCol; ++c) {
        QString h = firstRowCells.value(c);
        if (!h.isEmpty())
            headers.append(h);
    }

    return headers;
}

QList<OrderData> ExcelEngine::readExcel(const QString &filePath, const QMap<QString, int> &columnMapping)
{
    QList<OrderData> allOrders;

    if (!QFile::exists(filePath)) {
        emit errorOccurred(tr("文件不存在: %1").arg(filePath));
        return allOrders;
    }

    ZipReader zip(filePath);
    if (!zip.exists()) {
        emit errorOccurred(tr("无法打开文件: %1").arg(filePath));
        return allOrders;
    }

    QStringList sharedStrings = load_shared_strings_all(zip);
    QByteArray sheetXml = zip.fileData("xl/worksheets/sheet1.xml");

    int colWaybill = columnMapping.value("运单号", -1);
    int colTime = columnMapping.value("业务时间", -1);
    int colVol = columnMapping.value("体积重", -1);
    int colCustomer = columnMapping.value("订单客户", -1);
    int colClient = columnMapping.value("客户", -1);
    int colDest = columnMapping.value("目的省份", -1);
    int colWeight = columnMapping.value("实际重量", -1);
    int colFreight = columnMapping.value("运费", -1);
    int colUsedRule = columnMapping.value("使用的规则", -1);

    int maxCol = 0;
    for (int v : {colWaybill, colTime, colVol, colCustomer, colClient, colDest, colWeight, colFreight, colUsedRule}) {
        maxCol = qMax(maxCol, v);
    }
    maxCol += 2;

    int estRows = countRowsFast(filePath);
    if (estRows > 0)
        allOrders.reserve(estRows + 100);

    int currentRow = 0;
    QVector<QVariant> currentRowData(maxCol);
    QVector<bool> colSet(maxCol, false);

    int dataRowCount = 0;
    int progressInterval = qMax(1, estRows / 100);
    bool firstRow = true;

    auto processRow = [&]() {
        if (currentRow <= 0 || firstRow) return;

        OrderData order;
        order.isValid = true;

        if (colWaybill >= 0 && colSet[colWaybill]) {
            order.waybillNo = currentRowData[colWaybill].toString().trimmed();
        }
        if (colTime >= 0 && colSet[colTime]) {
            QVariant v = currentRowData[colTime];
            QDate date;
            if (v.userType() == QMetaType::QDate) {
                date = v.toDate();
            } else if (v.userType() == QMetaType::QDateTime) {
                date = v.toDateTime().date();
            } else {
                QString s = v.toString().trimmed();
                if (s.length() >= 10) {
                    date = QDate::fromString(s.left(10), "yyyy-MM-dd");
                    if (!date.isValid())
                        date = QDate::fromString(s.left(10), "yyyy/M/d");
                }
            }
            if (date.isValid())
                order.businessTime = date;
        }
        if (colVol >= 0 && colSet[colVol]) {
            bool ok;
            double d = currentRowData[colVol].toDouble(&ok);
            if (ok) order.volumetricWeight = d;
        }
        if (colCustomer >= 0 && colSet[colCustomer]) {
            order.customer = currentRowData[colCustomer].toString().trimmed();
        }
        if (colClient >= 0 && colSet[colClient]) {
            order.client = currentRowData[colClient].toString().trimmed();
        }
        if (colDest >= 0 && colSet[colDest]) {
            order.destinationProvince = currentRowData[colDest].toString().trimmed();
        }
        if (colWeight >= 0 && colSet[colWeight]) {
            bool ok;
            double d = currentRowData[colWeight].toDouble(&ok);
            if (ok) order.actualWeight = d;
        }
        if (colFreight >= 0 && colSet[colFreight]) {
            bool ok;
            double d = currentRowData[colFreight].toDouble(&ok);
            if (ok) order.freight = d;
        }
        if (colUsedRule >= 0 && colSet[colUsedRule]) {
            order.usedRule = currentRowData[colUsedRule].toString().trimmed();
        }

        // 过滤空行/无效行:
        // 1. 运单号为空 且 实际重量为0 且 体积重为0 → 跳过
        // 2. 运单号等于"运单号" → 重复表头行，跳过
        bool isEmptyRow = order.waybillNo.isEmpty() && order.actualWeight <= 0 && order.volumetricWeight <= 0;
        bool isDuplicateHeader = (order.waybillNo == QStringLiteral("运单号")) ||
                                 (order.waybillNo.trimmed().compare(QStringLiteral("运单号"), Qt::CaseInsensitive) == 0);
        if (isEmptyRow || isDuplicateHeader) {
            return;
        }

        allOrders.append(order);
        dataRowCount++;

        if (dataRowCount % progressInterval == 0) {
            int percent = estRows > 0 ? qMin(99, dataRowCount * 100 / estRows) : qMin(99, dataRowCount / 100);
            emit progressUpdated(percent);
        }
    };

    sax_options opt;
    opt.resolve_shared_strings = true;

    auto onCell = [&](const sax_cell& cell) -> bool {
        int r = cell.row;
        int c = cell.col - 1;

        if (r != currentRow) {
            processRow();
            firstRow = false;
            currentRow = r;
            for (int i = 0; i < maxCol; ++i) {
                currentRowData[i] = QVariant();
                colSet[i] = false;
            }
        }

        if (c >= 0 && c < maxCol) {
            currentRowData[c] = cell.value;
            colSet[c] = true;
        }

        return true;
    };

    read_sheet_xml_sax(sheetXml, opt, &sharedStrings, onCell);

    processRow();

    emit progressUpdated(100);
    return allOrders;
}

bool ExcelEngine::writeExcel(const QString &filePath, const QList<OrderData> &orders, const QStringList &headers)
{
    if (orders.size() > 50000) {
        return writeExcelFast(filePath, orders, headers);
    }

    QXlsx::Document xlsx;

    for (int c = 0; c < headers.size(); ++c) {
        xlsx.write(1, c + 1, headers[c]);
    }

    int totalOrders = orders.size();
    int progressInterval = qMax(1, totalOrders / 50);

    for (int i = 0; i < totalOrders; ++i) {
        const OrderData &order = orders[i];
        int row = i + 2;

        for (int c = 0; c < headers.size(); ++c) {
            const QString &header = headers[c];
            if (header == "运单号") {
                xlsx.write(row, c + 1, order.waybillNo);
            } else if (header == "业务时间") {
                xlsx.write(row, c + 1, order.businessTime.toString("yyyy-MM-dd"));
            } else if (header == "体积重") {
                xlsx.write(row, c + 1, order.volumetricWeight);
            } else if (header == "订单客户") {
                xlsx.write(row, c + 1, order.customer);
            } else if (header == "客户") {
                xlsx.write(row, c + 1, order.client);
            } else if (header == "目的省份") {
                xlsx.write(row, c + 1, order.destinationProvince);
            } else if (header == "实际重量") {
                xlsx.write(row, c + 1, order.actualWeight);
            } else if (header == "运费") {
                xlsx.write(row, c + 1, order.freight);
            } else if (header == "使用的规则") {
                xlsx.write(row, c + 1, order.usedRule);
            } else if (header == "错误信息") {
                xlsx.write(row, c + 1, order.errorMessage);
            }
        }

        if ((i + 1) % progressInterval == 0) {
            int percent = static_cast<int>((i + 1) * 100.0 / totalOrders);
            emit progressUpdated(percent);
        }
    }

    if (!xlsx.saveAs(filePath)) {
        emit errorOccurred(tr("保存文件失败: %1").arg(filePath));
        return false;
    }

    emit progressUpdated(100);
    return true;
}

static QString numToColStr(int col) {
    QString s;
    while (col > 0) {
        int rem = (col - 1) % 26;
        s.prepend(QChar('A' + rem));
        col = (col - 1) / 26;
    }
    return s;
}

static QString xmlEscape(const QString &s) {
    QString out;
    out.reserve(s.size() + 10);
    for (QChar c : s) {
        ushort uc = c.unicode();
        if (uc == '<') out += "&lt;";
        else if (uc == '>') out += "&gt;";
        else if (uc == '&') out += "&amp;";
        else if (uc == '"') out += "&quot;";
        else if (uc == '\'') out += "&apos;";
        else if (uc < 0x20 && uc != 0x09 && uc != 0x0A && uc != 0x0D) continue;
        else out += c;
    }
    return out;
}

bool ExcelEngine::writeExcelFast(const QString &filePath, const QList<OrderData> &orders, const QStringList &headers)
{
    int totalOrders = orders.size();
    int colCount = headers.size();

    // Excel单工作表最大行数(1,048,576)，减去表头行
    static constexpr int MAX_ROWS_PER_SHEET = 1048575;

    int sheetCount = (totalOrders + MAX_ROWS_PER_SHEET - 1) / MAX_ROWS_PER_SHEET;
    if (sheetCount < 1) sheetCount = 1;

    QStringList sharedStrings;
    QMap<QString, int> stringIndex;

    auto getStringIdx = [&](const QString &s) -> int {
        auto it = stringIndex.find(s);
        if (it != stringIndex.end())
            return it.value();
        int idx = sharedStrings.size();
        sharedStrings.append(s);
        stringIndex.insert(s, idx);
        return idx;
    };

    // 生成各工作表XML
    QList<QByteArray> sheetXmls;
    int progressInterval = qMax(1, totalOrders / 50);

    for (int sheetIdx = 0; sheetIdx < sheetCount; ++sheetIdx) {
        int startRow = sheetIdx * MAX_ROWS_PER_SHEET;
        int endRow = qMin(startRow + MAX_ROWS_PER_SHEET, totalOrders);
        int rowCount = (endRow - startRow) + 1;  // +1 for header

        QByteArray sheetXml;
        sheetXml.reserve(4096 + (endRow - startRow) * 200);
        QXmlStreamWriter w(&sheetXml);
        w.setAutoFormatting(false);
        w.writeStartDocument();
        w.writeStartElement("worksheet");
        w.writeAttribute("xmlns", "http://schemas.openxmlformats.org/spreadsheetml/2006/main");

        w.writeStartElement("sheetData");

        for (int r = 1; r <= rowCount; ++r) {
            w.writeStartElement("row");
            w.writeAttribute("r", QString::number(r));

            for (int c = 1; c <= colCount; ++c) {
                QString cellRef = numToColStr(c) + QString::number(r);

                if (r == 1) {
                    const QString &text = headers[c - 1];
                    int sIdx = getStringIdx(text);
                    w.writeStartElement("c");
                    w.writeAttribute("r", cellRef);
                    w.writeAttribute("t", "s");
                    w.writeTextElement("v", QString::number(sIdx));
                    w.writeEndElement();
                } else {
                    int orderIdx = startRow + (r - 2);
                    const OrderData &order = orders[orderIdx];
                    const QString &header = headers[c - 1];

                    if (header == "运单号") {
                        if (!order.waybillNo.isEmpty()) {
                            int sIdx = getStringIdx(order.waybillNo);
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeAttribute("t", "s");
                            w.writeTextElement("v", QString::number(sIdx));
                            w.writeEndElement();
                        }
                    } else if (header == "业务时间") {
                        if (order.businessTime.isValid()) {
                            double serial = 25569.0 + order.businessTime.startOfDay().toSecsSinceEpoch() / 86400.0;
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeAttribute("s", "1");
                            w.writeTextElement("v", QString::number(serial, 'f', 5));
                            w.writeEndElement();
                        }
                    } else if (header == "体积重") {
                        w.writeStartElement("c");
                        w.writeAttribute("r", cellRef);
                        w.writeTextElement("v", QString::number(order.volumetricWeight, 'f', 3));
                        w.writeEndElement();
                    } else if (header == "订单客户") {
                        if (!order.customer.isEmpty()) {
                            int sIdx = getStringIdx(order.customer);
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeAttribute("t", "s");
                            w.writeTextElement("v", QString::number(sIdx));
                            w.writeEndElement();
                        }
                    } else if (header == "客户") {
                        if (!order.client.isEmpty()) {
                            int sIdx = getStringIdx(order.client);
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeAttribute("t", "s");
                            w.writeTextElement("v", QString::number(sIdx));
                            w.writeEndElement();
                        }
                    } else if (header == "目的省份") {
                        if (!order.destinationProvince.isEmpty()) {
                            int sIdx = getStringIdx(order.destinationProvince);
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeAttribute("t", "s");
                            w.writeTextElement("v", QString::number(sIdx));
                            w.writeEndElement();
                        }
                    } else if (header == "实际重量") {
                        w.writeStartElement("c");
                        w.writeAttribute("r", cellRef);
                        w.writeTextElement("v", QString::number(order.actualWeight, 'f', 3));
                        w.writeEndElement();
                    } else if (header == "运费") {
                        if (order.freight > 0 || order.actualWeight > 0) {
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeTextElement("v", QString::number(order.freight, 'f', 2));
                            w.writeEndElement();
                        }
                    } else if (header == "使用的规则") {
                        if (!order.usedRule.isEmpty()) {
                            int sIdx = getStringIdx(order.usedRule);
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeAttribute("t", "s");
                            w.writeTextElement("v", QString::number(sIdx));
                            w.writeEndElement();
                        }
                    } else if (header == "错误信息") {
                        if (!order.errorMessage.isEmpty()) {
                            int sIdx = getStringIdx(order.errorMessage);
                            w.writeStartElement("c");
                            w.writeAttribute("r", cellRef);
                            w.writeAttribute("t", "s");
                            w.writeTextElement("v", QString::number(sIdx));
                            w.writeEndElement();
                        }
                    }
                }
            }

            w.writeEndElement();

            if (r > 1) {
                int globalRow = startRow + (r - 1);
                if (globalRow % progressInterval == 0) {
                    int percent = static_cast<int>(globalRow * 100.0 / totalOrders);
                    emit progressUpdated(qMin(99, percent));
                }
            }
        }

        w.writeEndElement();
        w.writeEndElement();
        w.writeEndDocument();

        sheetXmls.append(sheetXml);
    }

    // 生成 sharedStrings.xml
    QByteArray sharedStringsXml;
    QXmlStreamWriter sw(&sharedStringsXml);
    sw.setAutoFormatting(false);
    sw.writeStartDocument();
    sw.writeStartElement("sst");
    sw.writeAttribute("xmlns", "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    sw.writeAttribute("count", QString::number(sharedStrings.size()));
    sw.writeAttribute("uniqueCount", QString::number(sharedStrings.size()));
    for (const QString &s : sharedStrings) {
        sw.writeStartElement("si");
        sw.writeStartElement("t");
        QString escaped = xmlEscape(s);
        if (escaped != s) {
            sw.writeCharacters(escaped);
        } else {
            sw.writeCharacters(s);
        }
        sw.writeEndElement();
        sw.writeEndElement();
    }
    sw.writeEndElement();
    sw.writeEndDocument();

    QByteArray stylesXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
  <fonts count="1"><font><sz val="11"/><color theme="1"/><name val="Calibri"/><family val="2"/></font></fonts>
  <fills count="2"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill></fills>
  <borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders>
  <cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>
  <cellXfs count="3">
    <xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>
    <xf numFmtId="14" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>
    <xf numFmtId="49" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>
  </cellXfs>
  <cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>
  <dxfs count="0"/>
  <tableStyles count="0" defaultTableStyle="TableStyleMedium2" defaultPivotStyle="PivotStyleLight16"/>
</styleSheet>)";

    // 生成 workbook.xml (多工作表)
    QByteArray workbookXml;
    {
        QXmlStreamWriter wb(&workbookXml);
        wb.setAutoFormatting(false);
        wb.writeStartDocument();
        wb.writeStartElement("workbook");
        wb.writeAttribute("xmlns", "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
        wb.writeAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
        wb.writeStartElement("sheets");
        for (int i = 0; i < sheetCount; ++i) {
            QString name = sheetCount == 1 ? QStringLiteral("Sheet1") :
                           QStringLiteral("Sheet%1").arg(i + 1);
            wb.writeStartElement("sheet");
            wb.writeAttribute("name", name);
            wb.writeAttribute("sheetId", QString::number(i + 1));
            wb.writeAttribute("r:id", QStringLiteral("rId%1").arg(i + 1));
            wb.writeEndElement();
        }
        wb.writeEndElement();
        wb.writeEndElement();
        wb.writeEndDocument();
    }

    // 生成 workbook.xml.rels (多工作表关系)
    QByteArray workbookRels;
    {
        QXmlStreamWriter wr(&workbookRels);
        wr.setAutoFormatting(false);
        wr.writeStartDocument();
        wr.writeStartElement("Relationships");
        wr.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        for (int i = 0; i < sheetCount; ++i) {
            wr.writeStartElement("Relationship");
            wr.writeAttribute("Id", QStringLiteral("rId%1").arg(i + 1));
            wr.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet");
            wr.writeAttribute("Target", QStringLiteral("worksheets/sheet%1.xml").arg(i + 1));
            wr.writeEndElement();
        }
        wr.writeStartElement("Relationship");
        wr.writeAttribute("Id", QStringLiteral("rId%1").arg(sheetCount + 1));
        wr.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles");
        wr.writeAttribute("Target", "styles.xml");
        wr.writeEndElement();
        wr.writeStartElement("Relationship");
        wr.writeAttribute("Id", QStringLiteral("rId%1").arg(sheetCount + 2));
        wr.writeAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings");
        wr.writeAttribute("Target", "sharedStrings.xml");
        wr.writeEndElement();
        wr.writeEndElement();
        wr.writeEndDocument();
    }

    QByteArray rels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>)";

    // 生成 [Content_Types].xml (多工作表Override)
    QByteArray contentTypes;
    {
        QXmlStreamWriter ct(&contentTypes);
        ct.setAutoFormatting(false);
        ct.writeStartDocument();
        ct.writeStartElement("Types");
        ct.writeAttribute("xmlns", "http://schemas.openxmlformats.org/package/2006/content-types");
        ct.writeStartElement("Default");
        ct.writeAttribute("Extension", "rels");
        ct.writeAttribute("ContentType", "application/vnd.openxmlformats-package.relationships+xml");
        ct.writeEndElement();
        ct.writeStartElement("Default");
        ct.writeAttribute("Extension", "xml");
        ct.writeAttribute("ContentType", "application/xml");
        ct.writeEndElement();
        ct.writeStartElement("Override");
        ct.writeAttribute("PartName", "/xl/workbook.xml");
        ct.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
        ct.writeEndElement();
        for (int i = 0; i < sheetCount; ++i) {
            ct.writeStartElement("Override");
            ct.writeAttribute("PartName", QStringLiteral("/xl/worksheets/sheet%1.xml").arg(i + 1));
            ct.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
            ct.writeEndElement();
        }
        ct.writeStartElement("Override");
        ct.writeAttribute("PartName", "/xl/styles.xml");
        ct.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml");
        ct.writeEndElement();
        ct.writeStartElement("Override");
        ct.writeAttribute("PartName", "/xl/sharedStrings.xml");
        ct.writeAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml");
        ct.writeEndElement();
        ct.writeEndElement();
        ct.writeEndDocument();
    }

    // 写入zip
    ZipWriter zip(filePath);
    zip.addFile("[Content_Types].xml", contentTypes);
    zip.addFile("_rels/.rels", rels);
    zip.addFile("xl/workbook.xml", workbookXml);
    zip.addFile("xl/_rels/workbook.xml.rels", workbookRels);
    zip.addFile("xl/styles.xml", stylesXml);
    zip.addFile("xl/sharedStrings.xml", sharedStringsXml);
    for (int i = 0; i < sheetCount; ++i) {
        zip.addFile(QStringLiteral("xl/worksheets/sheet%1.xml").arg(i + 1).toUtf8(), sheetXmls[i]);
    }
    zip.close();

    if (zip.error()) {
        emit errorOccurred(tr("保存文件失败: %1").arg(filePath));
        return false;
    }

    emit progressUpdated(100);
    return true;
}

OrderData ExcelEngine::parseRow(const QList<QVariant> &rowData, const QMap<QString, int> &columnMap)
{
    OrderData order;
    order.isValid = true;

    auto getCellValue = [&](const QString &key) -> QVariant {
        if (!columnMap.contains(key))
            return QVariant();
        int col = columnMap[key];
        if (col < 0 || col >= rowData.size())
            return QVariant();
        return rowData[col];
    };

    QVariant waybillVal = getCellValue("运单号");
    if (!waybillVal.isNull())
        order.waybillNo = waybillVal.toString().trimmed();

    QVariant timeVal = getCellValue("业务时间");
    if (!timeVal.isNull() && !timeVal.toString().trimmed().isEmpty()) {
        QDate date;
        if (timeVal.userType() == QMetaType::QDate) {
            date = timeVal.toDate();
        } else if (timeVal.userType() == QMetaType::QDateTime) {
            date = timeVal.toDateTime().date();
        } else {
            QString timeStr = timeVal.toString().trimmed();
            static const QStringList formats = {
                "yyyy-MM-dd",
                "yyyy/M/d",
                "M/d/yyyy",
                "yyyy-MM-dd HH:mm:ss",
                "yyyy/M/d H:mm",
                "M/d/yyyy H:mm"
            };
            for (const auto &fmt : formats) {
                QDateTime dt = QDateTime::fromString(timeStr, fmt);
                if (dt.isValid()) {
                    date = dt.date();
                    break;
                }
            }
            if (!date.isValid()) {
                date = QDate::fromString(timeStr.left(10), "yyyy-MM-dd");
            }
        }
        if (date.isValid()) {
            order.businessTime = date;
        } else {
            order.isValid = false;
            order.errorMessage = "业务时间格式无法解析";
        }
    }

    QVariant volVal = getCellValue("体积重");
    if (!volVal.isNull()) {
        bool ok = false;
        double vol = volVal.toDouble(&ok);
        if (ok)
            order.volumetricWeight = vol;
    }

    QVariant customerVal = getCellValue("订单客户");
    if (!customerVal.isNull())
        order.customer = customerVal.toString().trimmed();

    QVariant clientVal = getCellValue("客户");
    if (!clientVal.isNull())
        order.client = clientVal.toString().trimmed();

    QVariant destVal = getCellValue("目的省份");
    if (!destVal.isNull())
        order.destinationProvince = destVal.toString().trimmed();

    QVariant weightVal = getCellValue("实际重量");
    if (!weightVal.isNull()) {
        bool ok = false;
        double w = weightVal.toDouble(&ok);
        if (ok)
            order.actualWeight = w;
    }

    QVariant freightVal = getCellValue("运费");
    if (!freightVal.isNull()) {
        bool ok = false;
        double f = freightVal.toDouble(&ok);
        if (ok)
            order.freight = f;
    }

    return order;
}
