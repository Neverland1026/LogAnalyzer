#ifndef PARSELOGTHREAD_H
#define PARSELOGTHREAD_H

#include <QThread>

class ParseLogThread : public QThread
{
    Q_OBJECT

public:

    ParseLogThread(QObject *parent = nullptr);
    ~ParseLogThread();

    // 设置需要解析的文件路径
    inline void setFilePath(const QString& path) { m_filePath = path; }

    // 设置需要解析的关键字
    inline void setKeywords(const QVector<QString>& keywords) { m_keywords = keywords; }

    // 设置大小写敏感
    inline void setCaseSensitive(bool flag) { m_caseSensitive = flag; }

    // 添加请求测试
    void increaseRequest() { ++m_requestCount; }

    // 结束解析
    inline void stop() { m_requestCount = 0; m_stoped = true; }

    // 核心函数
    void run() override;

signals:

    // 打开文件失败
    void sigOpenFileFailed();

    // 映射文件失败
    void sigMapFileFailed();

    // 发送解析的内容
    void sigParsedContent(const bool isIncrementalParse, const QString& full, const QString& part);

    // 解析完毕
    void sigParseFinished(int queryLineCount);

    // 开始解析
    void sigStart();

    // 解析终止
    void sigStop();

private:

    // 请求次数
    std::atomic<int> m_requestCount;

    // 是否终止线程
    std::atomic<bool> m_stoped;

    // 文件路径
    QString m_filePath;

    // 查找关键字
    QVector<QString> m_keywords;

    // 大小写是否敏感
    bool m_caseSensitive;

    // 上一次解析到的索引位置
    std::atomic<int> m_lastParseIndex;

};

#endif // PARSELOGTHREAD_H
