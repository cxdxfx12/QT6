#ifndef PROGRESSWIDGET_H
#define PROGRESSWIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

class ProgressWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProgressWidget(QWidget *parent = nullptr);

    void setProgress(int percent);
    void setStatusText(const QString &text);
    void setCancelButtonVisible(bool visible);

signals:
    void cancelled();

private:
    void setupUI();
    void applyDarkStyle();

    QProgressBar *m_progressBar;
    QLabel *m_percentLabel;
    QLabel *m_statusLabel;
    QPushButton *m_cancelButton;
};

#endif // PROGRESSWIDGET_H
