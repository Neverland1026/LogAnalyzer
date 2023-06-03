#include "FullScreenView.h"
#include "ui_FullScreenView.h"
#include <QDebug>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <QMenu>

FullScreenView::FullScreenView(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FullScreenView)
    , m_leftMousePressed(false)
    , m_ratio(9.0 / 16.0)
    , m_timer(new QTimer(this))
    , m_running(false)
{
    ui->setupUi(this);

    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);

    ::SetWindowPos((HWND)(this->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    this->resize(300, 300 * m_ratio);

    ui->textBrowser->setReadOnly(true);
    ui->textBrowser->setTextInteractionFlags(Qt::NoTextInteraction);

    ui->textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    ui->textBrowser->viewport()->installEventFilter(this);

    slotStateChanged(false);

    QObject::connect(ui->textBrowser, &QTextBrowser::cursorPositionChanged, this, [&]()
    {
        QTextCursor cursor =  ui->textBrowser->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->textBrowser->setTextCursor(cursor);
    });

    QObject::connect(m_timer, &QTimer::timeout, this, [&]()
    {
        static int deltaValue = 10;
        static int value = 0;

        if(value >= 250 || value <= 0)
        {
            deltaValue *= -1;
        }
        value += deltaValue;

        QString styleSheet = QString(
                    "QTextBrowser"                                \
                    "{"                                           \
                    "    padding-left:5;"                         \
                    "    padding-top:5;"                          \
                    "    padding-bottom:5;"                       \
                    "    padding-right:5;"                        \
                    "    background-color: rgb(238, 233, 233);"   \
                    "    border:2px solid;"                       \
                    "    border-color: rgba(255, 0, 0, %1);"      \
                    "}"
                    ).arg(QString::number(/*value*/255));
        ui->textBrowser->setStyleSheet(styleSheet);
    });
    //m_timer->setInterval(10);
    //m_timer->start();
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

    ui->textBrowser->append(part);
}

void FullScreenView::slotStateChanged(bool state)
{
    QString styleSheet;
    if(state)
    {
        styleSheet =
                "QTextBrowser"                                \
                "{"                                           \
                "    padding-left:5;"                         \
                "    padding-top:5;"                          \
                "    padding-bottom:5;"                       \
                "    padding-right:5;"                        \
                "    background-color: rgb(255, 255, 255);"   \
                "    border:2px solid;"                       \
                "    border-color: rgba(255, 0, 0, 255);"     \
                "}";
    }
    else
    {
        styleSheet =
                "QTextBrowser"                                \
                "{"                                           \
                "    padding-left:5;"                         \
                "    padding-top:5;"                          \
                "    padding-bottom:5;"                       \
                "    padding-right:5;"                        \
                "    background-color: rgb(255, 255, 255);"   \
                "    border:2px solid;"                       \
                "    border-color: rgba(0, 0, 255, 255);"     \
                "}";
    }
    ui->textBrowser->setStyleSheet(styleSheet);
}

bool FullScreenView::eventFilter(QObject* target, QEvent* event)
{
    if(target == ui->textBrowser->viewport())
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);

        if(mouseEvent->button() == Qt::RightButton)
        {
            QMenu* menu = new QMenu(this);
            QAction* clearAction = new QAction(QIcon(":/images/clear.svg"), QObject::tr("清除"), this);
            QAction* tagAction = new QAction(QIcon(":/images/tag.svg"), QObject::tr("添加标记"), this);
            QAction* exitAction = new QAction(QIcon(":/images/zoomout.svg"), QObject::tr("回到主界面"), this);
            menu->addAction(clearAction);
            menu->addSeparator();
            menu->addAction(tagAction);
            menu->addSeparator();
            menu->addAction(exitAction);

            QObject::connect(clearAction, &QAction::triggered, this, [&]()
            {
                ui->textBrowser->clear();
            });
            QObject::connect(tagAction, &QAction::triggered, this, [&]()
            {
                ui->textBrowser->append(m_splitSymbol);
            });
            QObject::connect(exitAction, &QAction::triggered, this, [&]()
            {
                emit sigExitFullScreen();
            });

            menu->exec(cursor().pos());
        }
        else if(wheelEvent->modifiers() == Qt::ControlModifier && mouseEvent->type() != QEvent::MouseMove)
        {
            static const int widthMinimum = this->width();
            static const int heightMinimum = this->height();

            static int deltaValue = 20;
            static int width = this->width();
            static int height = this->height();

            deltaValue = std::fabs(deltaValue) * ((wheelEvent->angleDelta().y() > 0) ? 1 : -1);

            width += deltaValue;
            height = width * m_ratio;

            if(width >= widthMinimum && height >= heightMinimum)
            {
                this->resize(width, height);
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
            else if (mouseEvent->type() == QEvent::MouseButtonDblClick && mouseEvent->button() == Qt::LeftButton)
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
    case Qt::Key_Enter:
    case Qt::Key_Return:
        ui->textBrowser->append(m_splitSymbol);
        break;
    case Qt::Key_Delete:
        ui->textBrowser->clear();
        break;
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
