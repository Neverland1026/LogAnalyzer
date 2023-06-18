#ifndef FULLSCREENVIEW_H
#define FULLSCREENVIEW_H

#include <QDialog>
#include <QCloseEvent>
#include <QTimer>

namespace Ui {
class FullScreenView;
}

class FullScreenView : public QDialog
{
    Q_OBJECT

public:
    explicit FullScreenView(QWidget *parent = nullptr);
    ~FullScreenView();

    // 处理解析内容
    void slotParsedContent(const bool isIncrementalParse, const QString& full, const QString& part);

    // 处理是否正在解析
    void slotStateChanged(bool running);

    // 设置分隔符
    inline void setSplitSymbol(const QString& splitSymbol) { m_splitSymbol = splitSymbol; }

    // 设置是否还在运行
    inline void setRunningState(bool state) { m_running = state; }

protected:

    // 事件过滤器
    bool eventFilter(QObject* target, QEvent* event) override;

    // 拦截 ESC 事件
    void keyPressEvent(QKeyEvent* event) override;

    // 窗口关闭
    void closeEvent(QCloseEvent* event) override;

signals:

    // 退出全屏
    void sigExitFullScreen();

private:
    Ui::FullScreenView *ui;

    // 鼠标左键按下
    bool m_leftMousePressed;

    // 窗口拖拽
    bool m_bDrag;
    QPointF m_mouseStartPoint;
    QPointF m_windowTopLeftPoint;

    // 高宽比
    double m_ratio;

    // 分割符
    QString m_splitSymbol;

    // 定时器
    QTimer* m_timer;

    // 运行状态
    bool m_running;

};

#endif // FULLSCREENVIEW_H
