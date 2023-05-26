#ifndef FULLSCREENVIEW_H
#define FULLSCREENVIEW_H

#include <QDialog>
#include <QCloseEvent>

namespace Ui {
class FullScreenView;
}

class FullScreenView : public QDialog
{
    Q_OBJECT

public:
    explicit FullScreenView(QWidget *parent = nullptr);
    ~FullScreenView();

    // 處理解析内容
    void slotParsedContent(const bool isIncrementalParse, const QString& full, const QString& part);

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

};

#endif // FULLSCREENVIEW_H
