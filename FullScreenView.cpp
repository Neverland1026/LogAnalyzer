#include "FullScreenView.h"
#include "ui_FullScreenView.h"
#include <QDebug>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

FullScreenView::FullScreenView(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FullScreenView)
    , m_leftMousePressed(false)
{
    ui->setupUi(this);

    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);

    ::SetWindowPos((HWND)(this->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    ui->textBrowser->setReadOnly(true);
    ui->textBrowser->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->textBrowser->setStyleSheet(
                "QTextBrowser { padding-left:5; padding-top:5; padding-bottom:5; padding-right:5 }"
                );
    ui->textBrowser->viewport()->installEventFilter(this);

    QObject::connect(ui->textBrowser, &QTextBrowser::cursorPositionChanged, this, [&]()
    {
        QTextCursor cursor =  ui->textBrowser->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->textBrowser->setTextCursor(cursor);
    });

    m_ratio = (double)(this->height()) / this->width();
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

bool FullScreenView::eventFilter(QObject* target, QEvent* event)
{
    if(target == ui->textBrowser->viewport())
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);

        if(wheelEvent->modifiers() == Qt::ControlModifier && mouseEvent->type() != QEvent::MouseMove)
        {
            static const int deltaValue = 20;
            if(wheelEvent->angleDelta().y() > 0)
            {
                this->resize(this->width() + deltaValue, this->height() + deltaValue * m_ratio);
            }
            else
            {
                this->resize(this->width() - deltaValue, this->height() - deltaValue * m_ratio);
            }

            return true;
        }
        else
        {
            if (mouseEvent->type() == QEvent::MouseButtonPress)
            {
                m_leftMousePressed = true;
                m_mouseStartPoint = mouseEvent->globalPosition();
                m_windowTopLeftPoint = this->frameGeometry().topLeft();
            }
            else if (mouseEvent->type() == QEvent::MouseButtonRelease)
            {
                m_leftMousePressed = false;
            }
            else if (mouseEvent->type() == QEvent::MouseMove)
            {
                if(m_leftMousePressed)
                {
                    QPointF distance = mouseEvent->globalPosition() - m_mouseStartPoint;
                    QPointF newDistance = m_windowTopLeftPoint + distance;

                    this->move(newDistance.x(), newDistance.y());
                }
            }
            else if (mouseEvent->type() == QEvent::MouseButtonDblClick)
            {
                emit sigExitFullScreen();
            }
        }
    }

    return QDialog::eventFilter(target, event);
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
