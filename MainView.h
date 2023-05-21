#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QDialog>
#include <QFileSystemWatcher>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "ParseLogThread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainView; }
QT_END_NAMESPACE

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

    // 重新跟踪文件
    void restartWatch();

    // 刷新搜索结果
    void refreshParseResult();

    // 设置部分按钮状态
    void setState(bool enabled);

    // 日志
    void LOG(const QString& log);

    // 文件拖拽
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    // 窗口关闭
    void closeEvent(QCloseEvent* event) override;

private:
    Ui::MainView *ui;

    // 监视的文件夹路径
    QString m_targetDirectory;

    // 监视的文件或文件前缀
    QString m_targetRegExp;

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

    // 所有解析结果
    QVector<QPair<QString, QString>> m_allParsedContent;

};
#endif // MAINVIEW_H
