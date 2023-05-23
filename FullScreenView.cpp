#include "FullScreenView.h"
#include "ui_FullScreenView.h"

#include "MainView.h"

FullScreenView::FullScreenView(MainView* parentView, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FullScreenView)
    , m_parentView(parentView)
{
    ui->setupUi(this);

    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
}

FullScreenView::~FullScreenView()
{
    delete ui;
}

void FullScreenView::slotParsedContent(const bool isIncrementalParse, const QString& full, const QString& part)
{
    if(!isIncrementalParse)
    {
        ui->textBrowser->clear();
    }

    ui->textBrowser->append(full);
}

//拖拽操作
void FullScreenView::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDrag = true;

        // 获得鼠标的初始位置
        m_mouseStartPoint = event->globalPosition();

        // 获得窗口的初始位置
        m_windowTopLeftPoint = this->frameGeometry().topLeft();
    }
}

void FullScreenView::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDrag)
    {
        // 获得鼠标移动的距离
        QPointF distance = event->globalPosition() - m_mouseStartPoint;
        QPointF newDistance = m_windowTopLeftPoint + distance;

        // 改变窗口的位置
        this->move(newDistance.x(), newDistance.y());
    }
}

void FullScreenView::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDrag = false;
    }
}

void FullScreenView::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Escape:
        emit sigExitFullScreen();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void FullScreenView::closeEvent(QCloseEvent* event)
{
    emit sigExitFullScreen();

    event->ignore();
}
