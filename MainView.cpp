#include "MainView.h"
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

MainView::MainView(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainView)
    , m_targetDirectory("")
    , m_targetRegExp("")
    , m_targetFile("")
    , m_targetKeywords({})
    , m_fileSystemWatcher(new QFileSystemWatcher(this))
    , m_parseLogThread(new ParseLogThread(this))
    , m_parseRunning(false)
    , m_allParsedContent({})
{
    ui->setupUi(this);

    init();

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
    this->setWindowFlags(windowFlags()& Qt::WindowMinMaxButtonsHint);//去除最大化窗

    // 读取配置文件
    [&]()
    {
        QScreen* screen = qApp->primaryScreen();
        QSettings settings("./config.ini", QSettings::Format::IniFormat);
        this->resize(settings.value("Config/WindowWidth", 1200).toInt(),
                     settings.value("Config/WindowHeight", 700).toInt());
        this->move(settings.value("Config/WindowX", (screen->size().width() - this->width()) / 2).toInt(),
                   settings.value("Config/WindowY", (screen->size().height() - this->height()) / 2).toInt());
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
        ui->lineEdit_targetDirectory->setText(QFileDialog::getExistingDirectory(this,
                                                                                QObject::tr("选择目标文件夹"),
                                                                                QDir::tempPath()));
    });

    // 目标文件名或前缀
    QObject::connect(ui->lineEdit_targetRegExp, &QLineEdit::textEdited, this, [&]()
    {
        // Nothing todo...
    });
    QObject::connect(ui->pushButton_targetRegExp, &QPushButton::clicked, this, [&]()
    {
        ui->lineEdit_targetRegExp->setText(QFileInfo(QFileDialog::getOpenFileName(this,
                                                                                  QObject::tr("选择目标文件"),
                                                                                  m_targetDirectory,
                                                                                  "Log File(*.LOG *.log)")).fileName());
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
            restartWatch();
        }
    });

    // 置顶
    QObject::connect(ui->pushButton_topHint, &QPushButton::clicked, this, [&]()
    {
        static bool topHint = false;
        topHint = !topHint;
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

    // 搜素结果设置
    QObject::connect(ui->checkBox_showLineNumber, &QCheckBox::clicked, this, [&]()
    {
        refreshParseResult();
    });
    QObject::connect(ui->checkBox_showFullContent, &QCheckBox::clicked, this, [&]()
    {
        refreshParseResult();
    });

    // 文件监视器
    QObject::connect(m_fileSystemWatcher, &QFileSystemWatcher::fileChanged, this, [&]()
    {
        static std::mutex s_mutex;
        std::lock_guard<std::mutex> lock(s_mutex);

        static size_t lastFileSize = 0;
        QFileInfo fi(m_targetFile);
        if(fi.size() > lastFileSize)
        {
            LOG("The located log file changed.");
            m_parseLogThread->increaseRequest();
        }
        else
        {
            // 曾经存在过跟踪的文件、但是出于某种原因现在没了
            if(m_targetFile != "" && !QFileInfo::exists(m_targetFile))
            {
                LOG("The located log file maybe removed, stop the parse thread.");

                m_targetFile = "";
                ui->textBrowser_parseResult->clear();
                ui->label_targetFile->setText("");
                if(m_parseLogThread->isRunning())
                {
                    m_parseLogThread->stop();
                }
            }
        }

        lastFileSize = fi.size();
    });
    QObject::connect(m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, [&]()
    {
        if(m_targetFile != "" && QFileInfo::exists(m_targetFile))
        {
            return;
        }

        LOG("The directory changed.");
        restartWatch();
    });

    // 解析线程
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
        if(!isIncrementalParse)
        {
            m_allParsedContent.resize(0);
        }
        m_allParsedContent.push_back({ full, part });
        ui->textBrowser_parseHistory->append(full);
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigParseFinished, this, [&](int queryLineCount)
    {
        LOG("Parse finished.");
        if(!m_targetFile.isEmpty())
        {
            ui->label_targetFile->setText(m_targetFile + "（ " + QObject::tr("已检索 %1").arg(queryLineCount) + " 行)");
        }
        refreshParseResult();
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigStart, this, [&]()
    {
        ui->progressBar->setMinimum(100);
        ui->progressBar->setMaximum(0);

        m_parseRunning = true;
        ui->pushButton_start->setIcon(QIcon(":/images/stop.svg"));
        setState(!m_parseRunning);

        LOG("Start new parse process.");
    });
    QObject::connect(m_parseLogThread, &ParseLogThread::sigStop, this, [&]()
    {
        ui->progressBar->setMinimum(0);
        ui->progressBar->setMaximum(100);
        ui->progressBar->setValue(0);

        m_parseRunning = false;
        ui->pushButton_start->setIcon(QIcon(":/images/start.svg"));
        setState(!m_parseRunning);

        LOG("Exit last parse process.");
        LOG(QString(50, '-'));
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
    /*ui->textBrowser_parseHistory->clear();*/
    /*ui->textBrowser_debug->clear();*/
}

void MainView::restartWatch()
{
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);

    LOG("Restart watch begin.");

    // 重置
    reset();

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
        LOG("[" + m_targetDirectory + "] is not exist.");
        QMessageBox::information(this,
                                 QObject::tr("提示"),
                                 QObject::tr("查询路径异常！"));
        return;
    }
    if(m_targetRegExp.isEmpty())
    {
        LOG("TargetRegExp is empty.");
        QMessageBox::information(this,
                                 QObject::tr("提示"),
                                 QObject::tr("查询文件名或前缀异常！"));
        return;
    }
    if(m_targetKeywords.isEmpty())
    {
        LOG("TargetKeywords is empty or invalid.");
        QMessageBox::information(this,
                                 QObject::tr("提示"),
                                 QObject::tr("查询关键字异常！"));
        return;
    }

    // 定义开始解析函数
    auto startParse = [&](const QString& filePath)
    {
        // 更新新文件对象
        m_targetFile = filePath;
        ui->label_targetFile->setText(m_targetFile);
        ui->textBrowser_parseHistory->append(QString(""));
        ui->textBrowser_parseHistory->append(QObject::tr(">>> 已定位文件: ") + ui->label_targetFile->text());

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

        m_parseLogThread->setFilePath(m_targetFile);
        m_parseLogThread->setKeywords(m_targetKeywords);
        m_parseLogThread->setCaseSensitive(ui->checkBox_caseSensitive->isChecked());
        \
        m_parseLogThread->increaseRequest();
        m_parseLogThread->start();
    };

    // 查找和 m_targetRegExp 最佳匹配的文件项
    [&]()
    {
        // 先直接判断文件是否存在
        const QString file = m_targetDirectory + "/" + m_targetRegExp;
        QFileInfo fi(file);
        if(fi.isFile() && fi.exists())
        {
            if(m_targetFile != file)
            {
                LOG("[" + file + "] is indeed exist, and ready to parse this log file.");
                startParse(file);
            }
            else
            {
                LOG("Same log file located and skip.");
            }
        }
        else if(ui->checkBox_regular->isChecked())
        {
            LOG("Prepare to query the most recently created log file.");

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
            LOG(QString::number(birthTimeMap.size()) + " related log file were found.");

            // 拿到最后一次创建的文件
            if(!birthTimeMap.isEmpty())
            {
                QMap<QDateTime, QString>::const_iterator it = birthTimeMap.constBegin();
                QString maybeNewFile = std::next(it, birthTimeMap.size() - 1).value();
                maybeNewFile = m_targetDirectory + "/" + maybeNewFile;
                if(maybeNewFile != m_targetFile)
                {
                    LOG("[" + maybeNewFile + "] is located, and ready to parse this log file.");
                    startParse(maybeNewFile);
                }
                else
                {
                    LOG("Same log file located and skip.");
                }
            }
            else
            {
                if(ui->checkBox_clearImmediately->isChecked())
                {
                    LOG("Clear parse result.");
                    ui->label_targetFile->setText("");
                    ui->textBrowser_parseResult->clear();
                }
            }
        }
    }();

    LOG("Restart watch end.");
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

void MainView::setState(bool enabled)
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
            ui->lineEdit_targetDirectory->setText(fi.path());
            ui->lineEdit_targetRegExp->setText(fi.fileName());
        }
    }
}

void MainView::closeEvent(QCloseEvent* event)
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
    settings.sync();

    reset();

    if(m_parseLogThread->isRunning())
    {
        m_parseLogThread->stop();
        m_parseLogThread->quit();
        m_parseLogThread->wait();
    }

    event->accept();
}
