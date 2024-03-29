﻿#include "MainView.h"
#include "ui_MainView.h"
#include <QFileDialog>
#include <QDir>
#include <QDateTime>
#include <QMimeData>
#include <QSettings>
#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <QMenu>

#include "FullScreenView.h"

MainView::MainView(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainView)
    , m_targetDirectory(QDir::tempPath())
    , m_targetRegExp("")
    , m_targetFile("")
    , m_targetKeywords({})
    , m_fileSystemWatcher(new QFileSystemWatcher(this))
    , m_parseLogThread(new ParseLogThread(this))
    , m_parseRunning(false)
    , m_timer(new QTimer())
    , m_allParsedContent({})
    , m_fullScreenView(new FullScreenView(this))
    , m_splitSymbol(QString("<font color=\"red\">%1</font>").arg(QString(30, '*')))
{
    ui->setupUi(this);

    init();

    m_fullScreenView->hide();
    m_fullScreenView->setSplitSymbol(m_splitSymbol);

    QTimer::singleShot(1000, this, [&]() {
        ui->textBrowser_parseResult->viewport()->installEventFilter(this);
    });

    LOG("Initial finished.");
}

MainView::~MainView()
{
    delete ui;
}

void MainView::init()
{
    reset();

    this->setWindowIcon(QIcon(":/images/logo.svg"));
    this->setAcceptDrops(true);
    this->setWindowFlags(windowFlags()& Qt::WindowMinMaxButtonsHint);

    // 读取配置文件
    [&]()
    {
        QScreen* screen = qApp->primaryScreen();
        QSettings settings("./config.ini", QSettings::Format::IniFormat);
        this->resize(settings.value("Config/WindowWidth", 900).toInt(),
                     settings.value("Config/WindowHeight", 550).toInt());
        this->move(settings.value("Config/WindowX", (screen->size().width() - this->width()) / 2).toInt(),
                   settings.value("Config/WindowY", (screen->size().height() - this->height()) / 2).toInt());
        if((settings.value("Config/TopHint", false).toBool()))
        {
            ui->pushButton_topHint->animateClick();
        }
        ui->lineEdit_targetDirectory->setText(settings.value("Config/DirectoryPath", QDir::tempPath()).toString());
        ui->lineEdit_targetRegExp->setText(settings.value("Config/RegExp", "").toString());
        ui->lineEdit_targetKeywords->setText(settings.value("Config/Keywords", "").toString());
        ui->checkBox_caseSensitive->setChecked(settings.value("Config/CaseSensitive", true).toBool());
        ui->checkBox_regular->setChecked(settings.value("Config/Regular", true).toBool());
        ui->checkBox_showLineNumber->setChecked(settings.value("Config/ShowLineNumber", false).toBool());
        ui->checkBox_showFullContent->setChecked(settings.value("Config/ShowFullContent", false).toBool());
        ui->checkBox_clearImmediately->setChecked(settings.value("Config/ClearImmediately", false).toBool());
    }();

    // 目标搜索路径
    QObject::connect(ui->lineEdit_targetDirectory, &QLineEdit::textEdited, this, [&]()
    {
        // Nothing todo...
    });
    QObject::connect(ui->pushButton_targetDirectory, &QPushButton::clicked, this, [&]()
    {
        QString directory = QFileDialog::getExistingDirectory(this,
                                                              QObject::tr("选择目标文件夹"),
                                                              m_targetDirectory);
        if(false == directory.isEmpty())
        {
            m_targetDirectory = directory;
            ui->lineEdit_targetDirectory->setText(m_targetDirectory);
        }
    });

    // 目标文件名或前缀
    QObject::connect(ui->lineEdit_targetRegExp, &QLineEdit::textEdited, this, [&]()
    {
        // Nothing todo...
    });
    QObject::connect(ui->pushButton_targetRegExp, &QPushButton::clicked, this, [&]()
    {
        QString file = QFileDialog::getOpenFileName(this,
                                                    QObject::tr("选择目标文件"),
                                                    m_targetDirectory,
                                                    "Log files(*.LOG *.log);;All files(*.*)");
        if(QFileInfo::exists(file))
        {
            m_targetDirectory = QFileInfo(file).filePath();
            ui->lineEdit_targetDirectory->setText(m_targetDirectory);
            ui->lineEdit_targetRegExp->setText(QFileInfo(file).fileName());
        }
    });

    // 搜索关键字
    QObject::connect(ui->lineEdit_targetKeywords, &QLineEdit::textEdited, this, [&]()
    {
        // Nothing todo...
    });

    // 开始与结束
    QObject::connect(ui->pushButton_start, &QPushButton::clicked, this, [&]()
    {
        if(m_parseRunning)
        {
            if(m_parseLogThread->isRunning())
            {
                m_parseLogThread->stop();
            }
        }
        else
        {
            reset();

            m_targetFile = "";

            // 汇总用户输入的信息
            [&]()
            {
                m_targetDirectory = ui->lineEdit_targetDirectory->text();
                m_targetRegExp = ui->lineEdit_targetRegExp->text();
                m_targetKeywords.clear();
                QStringList qstrList = ui->lineEdit_targetKeywords->text().split(";");
                for(int i = 0; i <= qstrList.size() - 1; ++i)
                {
                    if(!qstrList[i].isEmpty())
                    {
                        m_targetKeywords.push_back(qstrList[i]);
                    }
                }
            }();

            // 判断输入有效性
            if(m_targetDirectory.isEmpty() || !QDir(m_targetDirectory).exists())
            {
                return topWarning(QObject::tr("查询路径异常！"));
            }
            if(m_targetRegExp.isEmpty())
            {
                return topWarning(QObject::tr("查询表达式异常！"));
            }
            if(m_targetKeywords.isEmpty())
            {
                return topWarning(QObject::tr("查询关键字异常！"));
            }

            QString detectFile;
            if(detectNewFile(detectFile, true))
            {
                m_targetFile = detectFile;
                restartWatch();
            }
        }
    });

    // 置顶
    QObject::connect(ui->pushButton_topHint, &QPushButton::clicked, this, [&]()
    {
        static bool topHint = false;
        topHint = !topHint;
        ui->pushButton_topHint->setProperty("topHint", topHint);

        if(topHint)
        {
            ui->pushButton_topHint->setIcon(QIcon(":/images/topHint_active.svg"));
            ::SetWindowPos((HWND)(this->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
        else
        {
            ui->pushButton_topHint->setIcon(QIcon(":/images/topHint_normal.svg"));
            ::SetWindowPos((HWND)(this->winId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    });

    // 迷你窗口
    QObject::connect(ui->pushButton_zoom_in, &QPushButton::clicked, this, [&]()
    {
        this->hide();
        m_fullScreenView->show();
    });

    // 搜素结果设置
    QObject::connect(ui->checkBox_showLineNumber, &QCheckBox::clicked, this, [&]()
    {
        refreshParseResult();
    });
    QObject::connect(ui->checkBox_showFullContent, &QCheckBox::clicked, this, [&]()
    {
        refreshParseResult();
    });
    QObject::connect(ui->checkBox_clearImmediately, &QCheckBox::clicked, this, [&]()
    {
        // Nothing todo...
    });

    // QTextBrowser
    QObject::connect(ui->textBrowser_parseResult, &QTextBrowser::cursorPositionChanged, this, [&]()
    {
        //QTextCursor cursor =  ui->textBrowser_parseResult->textCursor();
        //cursor.movePosition(QTextCursor::End);
        //ui->textBrowser_parseResult->setTextCursor(cursor);
    });

    // 文件监视器
    QObject::connect(m_fileSystemWatcher, &QFileSystemWatcher::fileChanged, this, [&]()
    {
        static std::mutex s_mutex;
        std::lock_guard<std::mutex> lock(s_mutex);

        static size_t s_lastFileSize = 0;
        QFileInfo fi(m_targetFile);
        if(fi.size() > s_lastFileSize)
        {
            /*LOG("The located log file's content changed.");*/
            m_parseLogThread->increaseRequest();
        }
        else
        {
            // 曾经存在过跟踪的文件、但是出于某种原因现在没了
            if(m_targetFile != "" && !QFileInfo::exists(m_targetFile))
            {
                LOG("The located log file maybe removed.");

                s_lastFileSize = 0;
                m_targetFile = "";
                ui->label_targetFile->setText(QObject::tr("已定位文件:   "));

                if(/*ui->checkBox_clearImmediately->isChecked()*/true)
                {
                    ui->textBrowser_parseResult->clear();
                }
            }
        }

        s_lastFileSize = fi.size();
    });
    QObject::connect(m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, [&]()
    {
        if(m_parseRunning)
        {
            QString detectFile;
            if(detectNewFile(detectFile))
            {
                m_targetFile = detectFile;

                m_allParsedContent.resize(0);

                // 这个必须要强制删除
                if(/*ui->checkBox_clearImmediately->isChecked()*/true)
                {
                    ui->textBrowser_parseResult->clear();
                }

                restartWatch();
            }
        }
    });

    // 解析线程
    QObject::connect(m_parseLogThread, &ParseLogThread::sigFileExist, this, [&]()
    {
        emit sigStateChanged(true);
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigOpenFileFailed, this, [&]()
    {
        LOG("Open file failed.");
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigMapFileFailed, this, [&]()
    {
        LOG("File mapping failed.");
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigParsedContent, this,
                     [&](const bool isIncrementalParse, const QString& full, const QString& part)
    {
        emit sigParsedContent(isIncrementalParse, full, part);

        if(!isIncrementalParse)
        {
            m_allParsedContent.resize(0);
        }

        m_allParsedContent.push_back({ full, part });
        ui->textBrowser_parseHistory->append(full);
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigParseFinished, this, [&](int queryLineCount)
    {
        if(!m_targetFile.isEmpty())
        {
            ui->label_targetFile->setText(QObject::tr("已定位文件:   ") + QFileInfo(m_targetFile).fileName() + "（ " + QObject::tr("已检索 %1").arg(queryLineCount) + " 行)");
        }
        refreshParseResult();
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigStart, this, [&]()
    {
        ui->progressBar->setMinimum(100);
        ui->progressBar->setMaximum(0);

        m_timer->start();

        m_parseRunning = true;
        ui->pushButton_start->setIcon(QIcon(":/images/stop.svg"));
        setUIEnabled(!m_parseRunning);
        /*emit sigStateChanged(true);*/

        LOG("Parse process begin.");
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigStop, this, [&]()
    {
        ui->progressBar->setMinimum(0);
        ui->progressBar->setMaximum(100);
        ui->progressBar->setValue(0);

        m_timer->stop();

        m_parseRunning = false;
        ui->pushButton_start->setIcon(QIcon(":/images/start.svg"));
        setUIEnabled(!m_parseRunning);
        emit sigStateChanged(false);

        LOG("Parse process end.");
    });

    // m_timer
    m_timer->setInterval(1000);
    QObject::connect(m_timer, &QTimer::timeout, this, [&]()
    {
        if(m_targetFile != "" && QFileInfo::exists(m_targetFile))
        {
            FILE* file;
            file = fopen(m_targetFile.toStdString().data(), "r");
            if(file)
            {
                HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
                ::FlushFileBuffers(hFile);

                fclose(file);
            }
        }
    });

    // m_fullScreenView
    QObject::connect(this, &MainView::sigParsedContent, m_fullScreenView, &FullScreenView::slotParsedContent);
    QObject::connect(this, &MainView::sigStateChanged, m_fullScreenView, &FullScreenView::slotStateChanged);
    QObject::connect(m_fullScreenView, &FullScreenView::sigExitFullScreen, this, [&]()
    {
        m_fullScreenView->hide();
        this->show();
    });
}

void MainView::reset()
{
    ui->lineEdit_targetDirectory->setReadOnly(true);

    m_targetDirectory.clear();
    m_targetRegExp.clear();
    /*m_targetFile.clear();*/  // 不能删除
    m_targetKeywords.clear();
    m_allParsedContent.resize(0);
    ui->textBrowser_parseResult->clear();
}

bool MainView::detectNewFile(QString& detectFile, bool allowVirtualFile /*= false*/)
{
    // 先直接判断文件是否存在
    const QString file = m_targetDirectory + "/" + m_targetRegExp;
    QFileInfo fi(file);
    if(fi.isFile() && fi.exists())
    {
        if(file != m_targetFile)
        {
            detectFile = file;
            return true;
        }
    }
    else if(ui->checkBox_regular->isChecked())
    {
        // 说明 m_targetRegExp 此时代表的是文件前缀，那么需要查找包含该前缀的最近更新的文件
        const QStringList allFiles = QDir(m_targetDirectory).entryList(QStringList({ "*.LOG", "*.log" }),
                                                                       QDir::Files | QDir::Readable,
                                                                       QDir::Name);

        // 按创建时间排序
        QMap<QDateTime, QString> birthTimeMap;
        for(const auto& file : allFiles)
        {
            QFileInfo fi(file);
            if(fi.baseName().contains(m_targetRegExp))
            {
                birthTimeMap.insert(QFileInfo(file).birthTime(), file);
            }
        }
        /*LOG(QString::number(birthTimeMap.size()) + " related log file were found.");*/

        // 拿到最后一次创建的文件
        if(!birthTimeMap.isEmpty())
        {
            QMap<QDateTime, QString>::const_iterator it = birthTimeMap.constBegin();
            QString maybeNewFile = std::next(it, birthTimeMap.size() - 1).value();
            maybeNewFile = m_targetDirectory + "/" + maybeNewFile;
            if(maybeNewFile != m_targetFile)
            {
                detectFile = maybeNewFile;
                return true;
            }
        }
        else
        {
            if(allowVirtualFile)
            {
                detectFile = "";
                return true;
            }
        }
    }

    return false;
}

void MainView::restartWatch()
{
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);

    // 开始解析
    ui->label_targetFile->setText(QObject::tr("已定位文件:   ") + QFileInfo(m_targetFile).fileName());
    flickBackground();
    ui->textBrowser_parseHistory->append(QString(""));
    ui->textBrowser_parseHistory->append(QObject::tr(">>> ") + ui->label_targetFile->text());

    m_fileSystemWatcher->removePaths(m_fileSystemWatcher->files());
    m_fileSystemWatcher->removePaths(m_fileSystemWatcher->directories());
    m_fileSystemWatcher->addPath(m_targetDirectory);
    m_fileSystemWatcher->addPath(m_targetFile);

    if(m_parseLogThread->isRunning())
    {
        m_parseLogThread->stop();
        m_parseLogThread->quit();
        m_parseLogThread->wait();
    }

    m_fullScreenView->clear();

    m_parseLogThread->setFilePath(m_targetFile);
    m_parseLogThread->setKeywords(m_targetKeywords);
    m_parseLogThread->setCaseSensitive(ui->checkBox_caseSensitive->isChecked());

    m_parseLogThread->increaseRequest();
    m_parseLogThread->start();

    QApplication::alert(this, 3000);
}

void MainView::refreshParseResult()
{
    // 判断一个数字是几位数
    auto bitNumber = [](int n)
    {
        if(n == 0) return 1;

        int count = 0;
        while(n != 0)
        {
            n /= 10;
            ++count;
        }

        return count;
    };

    // 总位数
    const int BitSize = bitNumber(m_allParsedContent.size());

    // 输出行号
    auto lineNumber = [&](int index)
    {
        if(ui->checkBox_showLineNumber->isChecked())
        {
            return QString("[") + QString(BitSize - bitNumber(index), '0') + QString::number(index) + QString("]");
        }
        else
        {
            return QString("");
        }
    };

    // 实际内容
    auto realContent = [&](int index)
    {
        return ui->checkBox_showFullContent->isChecked() ? m_allParsedContent[index].first : m_allParsedContent[index].second;
    };

    // 输出
    ui->textBrowser_parseResult->clear();
    for(int i = 0; i < m_allParsedContent.size(); ++i)
    {
        ui->textBrowser_parseResult->append(lineNumber(i + 1) + " " + realContent(i));
    }
}

void MainView::setUIEnabled(bool enabled)
{
    ui->lineEdit_targetDirectory->setEnabled(enabled);
    ui->pushButton_targetDirectory->setEnabled(enabled);
    ui->lineEdit_targetRegExp->setEnabled(enabled);
    ui->pushButton_targetRegExp->setEnabled(enabled);
    ui->lineEdit_targetKeywords->setEnabled(enabled);
}

void MainView::LOG(const QString& log)
{
    ui->textBrowser_debug->append(">>> " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzzzzz") + "    " + log);
}

void MainView::topWarning(const QString& info)
{
    QMessageBox messageBox(QMessageBox::Question, QObject::tr("提示"), info, QMessageBox::Yes, this);
    messageBox.button(QMessageBox::Yes)->setText(QObject::tr("确定"));
    messageBox.exec();
}

bool MainView::topQuestion(const QString& info)
{
    QMessageBox messageBox(QMessageBox::Question, QObject::tr("提示"), info, QMessageBox::Yes | QMessageBox::No, this);
    messageBox.button(QMessageBox::Yes)->setText(QObject::tr("确定"));
    messageBox.button(QMessageBox::No)->setText(QObject::tr("取消"));

    return (messageBox.exec() == QMessageBox::Yes);
}

void MainView::flickBackground()
{
    static QTimer s_timer = QTimer();

    if(s_timer.isActive())
    {
        return;
    }

    QObject::connect(&s_timer, &QTimer::timeout, this, [&]()
    {
        static const int unit_value = 10;
        static int deltaValue = unit_value;
        static int value = unit_value * 2;
        static int count = 2 * 3;

        bool leave = false;

        if(value >= 250 - unit_value || value <= unit_value)
        {
            deltaValue = deltaValue * (-1);
            --count;

            if(count == 0)
            {
                count = 2 * 3;
                leave = true;
            }
        }
        value += deltaValue;

        QString styleSheet = QString(
                                 "QLabel"                                  \
                                 "{"                                       \
                                 "    background: \"#FFFAF0\";"            \
                                 "    color: \"#000000\";"                 \
                                 "    border:3px solid;"                   \
                                 "    border-color: rgba(255, 0, 0, %1);"  \
                                 "}"
                                 ).arg(QString::number(leave ? 0 : value));

        ui->label_targetFile->setStyleSheet(styleSheet);

        if(leave)
        {
            s_timer.stop();
            QObject::disconnect(&s_timer, 0, 0, 0);
        }
    });

    s_timer.setInterval(25);
    s_timer.start();
}

void MainView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
    else
    {
        event->ignore();
    }
}

void MainView::dropEvent(QDropEvent* event)
{
    if(m_parseRunning)
    {
        return;
    }

    // 获取MIME数据
    const QMimeData *mimeData = event->mimeData();

    // 如果数据中包含 URL
    if(mimeData->hasUrls())
    {
        // 获取URL列表
        QList<QUrl> urlList = mimeData->urls();

        // 将其中的第一个 URL 表示为本地文件路径，toLocalFile() 转换为本地文件路径
        QString fileName = urlList.at(0).toLocalFile();
        QFileInfo fi(fileName);
        if(fi.exists())
        {
            QLineEdit* child = static_cast<QLineEdit*>(childAt(event->position().toPoint()));
            if(Q_NULLPTR != child)
            {
                if(child == ui->lineEdit_targetDirectory)
                {
                    if(fi.isDir())
                    {
                        m_targetDirectory = fileName;
                        ui->lineEdit_targetDirectory->setText(m_targetDirectory);
                    }
                }
                else if(child == ui->lineEdit_targetRegExp)
                {
                    if(fi.isFile())
                    {
                        m_targetDirectory = fi.absolutePath();
                        ui->lineEdit_targetDirectory->setText(m_targetDirectory);
                        ui->lineEdit_targetRegExp->setText(fi.fileName());
                    }
                }
            }
        }
    }
}

void MainView::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        m_allParsedContent.emplace_back(m_splitSymbol, m_splitSymbol);
        ui->textBrowser_parseResult->append(m_splitSymbol);
        break;
    case Qt::Key_Delete:
        m_allParsedContent.resize(0);
        ui->textBrowser_parseResult->clear();
        break;
    case Qt::Key_Escape:
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void MainView::closeEvent(QCloseEvent* event)
{
    if(topQuestion(QObject::tr("确定要退出吗？")))
    {
        QSettings settings("./config.ini", QSettings::Format::IniFormat);
        settings.setValue("Config/WindowWidth", this->width());
        settings.setValue("Config/WindowHeight", this->height());
        settings.setValue("Config/WindowX", this->pos().x());
        settings.setValue("Config/WindowY", this->pos().y());
        settings.setValue("Config/DirectoryPath", ui->lineEdit_targetDirectory->text());
        settings.setValue("Config/RegExp", ui->lineEdit_targetRegExp->text());
        settings.setValue("Config/Keywords", ui->lineEdit_targetKeywords->text());
        settings.setValue("Config/CaseSensitive", ui->checkBox_caseSensitive->isChecked());
        settings.setValue("Config/Regular", ui->checkBox_regular->isChecked());
        settings.setValue("Config/ShowLineNumber", ui->checkBox_showLineNumber->isChecked());
        settings.setValue("Config/ShowFullContent", ui->checkBox_showFullContent->isChecked());
        settings.setValue("Config/ClearImmediately", ui->checkBox_clearImmediately->isChecked());
        settings.setValue("Config/TopHint", ui->pushButton_topHint->property("topHint").toBool());
        ui->pushButton_topHint->clicked(settings.value("Config/TopHint", false).toBool());
        settings.sync();

        if(m_parseLogThread->isRunning())
        {
            m_parseLogThread->stop();
            m_parseLogThread->quit();
            m_parseLogThread->wait();
        }

        if(m_timer->isActive())
        {
            m_timer->stop();
        }

        event->accept();
    }
    else
    {
        event->ignore();
    }
}

bool MainView::eventFilter(QObject* target, QEvent* event)
{
    if(target == ui->textBrowser_parseResult->viewport())
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

        if(mouseEvent->button() == Qt::RightButton)
        {
            QMenu* menu = new QMenu(this);
            QAction* clearAction = new QAction(QIcon(":/images/clear.svg"), QObject::tr("清除"), this);
            QAction* tagAction = new QAction(QIcon(":/images/tag.svg"), QObject::tr("添加标记"), this);
            menu->addAction(clearAction);
            menu->addSeparator();
            menu->addAction(tagAction);

            QObject::connect(clearAction, &QAction::triggered, this, [&]()
            {
                m_allParsedContent.resize(0);
                ui->textBrowser_parseResult->clear();
            });
            QObject::connect(tagAction, &QAction::triggered, this, [&]()
            {
                m_allParsedContent.emplace_back(m_splitSymbol, m_splitSymbol);
                ui->textBrowser_parseResult->append(m_splitSymbol);
            });

            menu->exec(cursor().pos());
        }
    }

    return QDialog::eventFilter(target, event);
}
