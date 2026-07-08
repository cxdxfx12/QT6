#include "ExcelImporter.h"
#include "ExcelEngine.h"

ExcelImporter::ExcelImporter(QObject *parent)
    : QObject(parent)
    , m_engine(new ExcelEngine(this))
{
    connect(m_engine, &ExcelEngine::progressUpdated, this, &ExcelImporter::progressUpdated);
    connect(m_engine, &ExcelEngine::errorOccurred, this, &ExcelImporter::errorOccurred);
}

QList<OrderData> ExcelImporter::importFromFile(const QString &filePath)
{
    QStringList headers = m_engine->getHeaders(filePath);
    if (headers.isEmpty()) {
        emit errorOccurred(tr("无法获取表头或文件为空"));
        return {};
    }

    QMap<QString, int> columnMapping = autoDetectColumns(headers);
    if (columnMapping.isEmpty()) {
        emit errorOccurred(tr("自动列映射失败，未匹配到任何必需列"));
        return {};
    }

    return m_engine->readExcel(filePath, columnMapping);
}

QList<OrderData> ExcelImporter::importFromFile(const QString &filePath, const QMap<QString, int> &columnMapping)
{
    return m_engine->readExcel(filePath, columnMapping);
}

QStringList ExcelImporter::getAvailableHeaders(const QString &filePath)
{
    return m_engine->getHeaders(filePath);
}

QMap<QString, int> ExcelImporter::autoDetectColumns(const QStringList &headers)
{
    QMap<QString, int> mapping;

    static const QMap<QString, QStringList> keywordMap = {
        {"运单号",   {"运单号", "快递单号", "单号", "运单编号", "快件编号", "物流单号", "面单号"}},
        {"业务时间", {"业务时间", "时间", "日期", "业务日期", "下单时间", "订单时间", "发货时间"}},
        {"体积重",   {"体积重", "体积重量", "体积", "抛重", "抛货重量"}},
        {"订单客户", {"订单客户", "寄件客户", "下单客户", "发货客户", "发件客户"}},
        {"客户",     {"结算对象", "结算客户", "客户", "付款客户", "结算方", "收款对象", "结算主体", "结算单位"}},
        {"目的省份", {"目的省份", "省份", "收件省份", "目的省", "到达省份", "到达省", "收货省份"}},
        {"实际重量", {"结算重量", "计费重量", "核算重量", "实际重量", "实重", "重量", "实重(kg)", "实际重", "快件重量"}},
        {"运费",     {"运费", "费用", "金额", "运费(元)", "应收运费"}},
        {"使用的规则", {"使用的规则", "规则", "计算规则", "适用规则", "匹配规则"}},
        {"错误信息", {"错误信息", "错误", "备注", "异常信息", "备注信息"}}
    };

    for (auto it = keywordMap.constBegin(); it != keywordMap.constEnd(); ++it) {
        const QString &targetField = it.key();
        const QStringList &keywords = it.value();

        int bestIndex = -1;
        int bestPriority = -1;  // 优先级: 精确匹配 > 长关键词 > 短关键词

        for (int i = 0; i < headers.size(); ++i) {
            const QString &header = headers[i];
            // 跳过已被其他字段占用的列(允许重复，但优先未占用)
            for (int k = 0; k < keywords.size(); ++k) {
                const QString &keyword = keywords[k];
                if (header.contains(keyword, Qt::CaseInsensitive)) {
                    // 优先级: 精确匹配(100) > 长关键词(50+keyword长度) > 短关键词(keyword长度)
                    int priority;
                    if (header == keyword || header.trimmed() == keyword) {
                        priority = 100;
                    } else {
                        priority = 50 + keyword.length();
                    }
                    if (priority > bestPriority) {
                        bestIndex = i;
                        bestPriority = priority;
                    }
                    break;
                }
            }
        }

        if (bestIndex >= 0) {
            mapping[targetField] = bestIndex;
        }
    }

    return mapping;
}
