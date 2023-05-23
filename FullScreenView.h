﻿#ifndef FULLSCREENVIEW_H
#define FULLSCREENVIEW_H

#include <QDialog>
#include <QCloseEvent>

namespace Ui {
class FullScreenView;
}

class MainView;

class FullScreenView : public QDialog
{
    Q_OBJECT

public:
    explicit FullScreenView(MainView* parentView, QWidget *parent = nullptr);
    ~FullScreenView();

    // 處理解析内容
    void slotParsedContent(const bool isIncrementalParse, const QString& full, const QString& part);

protected:

    //拖拽窗口
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    // 拦截 ESC 事件
    void keyPressEvent(QKeyEvent* event);

    // 窗口关闭
    void closeEvent(QCloseEvent* event) override;

signals:

    // 退出全屏
    void sigExitFullScreen();

private:
    Ui::FullScreenView *ui;

    // 主窗口
    MainView* m_parentView;

    // 窗口拖拽
    bool m_bDrag;
    QPointF m_mouseStartPoint;
    QPointF m_windowTopLeftPoint;

};

#endif // FULLSCREENVIEW_H