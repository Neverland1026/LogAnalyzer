#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QDialog>
#include <QFileSystemWatcher>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QTimer>
#include "ParseLogThread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainView; }
QT_END_NAMESPACE

class FullScreenView;

class MainView : public QDialog
{
    Q_OBJECT

public:
    MainView(QWidget *parent = nullptr);
    ~MainView();

    // 初始化
    void init();

protected:

    // 重置
    void reset();

    // 是否有新文件
    bool detectNewFile(QString& newFile);

    // 重新跟踪文件
    void restartWatch();

    // 刷新搜索结果
    void refreshParseResult();

    // 设置部分按钮状态
    void setUIEnabled(bool enabled);

    // 日志
    void LOG(const QString& log);

    // 警告提示
    void topWarning(const QString& info);

    // 询问提示
    bool topQuestion(const QString& info);

    // 背景闪烁
    void flickBackground();

    // 文件拖拽
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    // 拦截 ESC 事件
    void keyPressEvent(QKeyEvent* event) override;

    // 窗口关闭
    void closeEvent(QCloseEvent* event) override;

    // 事件过滤器
    bool eventFilter(QObject* target, QEvent* event) override;

signals:

    // 发送解析的内容
    void sigParsedContent(const bool isIncrementalParse, const QString& full, const QString& part);

    // 发送状态改变
    void sigStateChanged(bool running);

private:
    Ui::MainView *ui;

    // 监视的文件夹路径
    QString m_targetDirectory;

    // 监视的文件或文件前缀
    QString m_targetFileFuzzy;

    // 真正监视的文件
    QString m_targetFile;

    // 要输出的关键字
    QVector<QString> m_targetKeywords;

    // 监视对象
    QFileSystemWatcher* m_fileSystemWatcher;

    // 解析线程
    ParseLogThread* m_parseLogThread;

    // 是否正在解析
    bool m_parseRunning;

    // 实时将缓冲区数据写到本地
    QTimer* m_timer;

    // 所有解析结果
    QVector<QPair<QString, QString>> m_allParsedContent;

    // 全屏窗口
    FullScreenView* m_fullScreenView;

    // 分割符
    const QString m_splitSymbol;

};
#endif // MAINVIEW_H
