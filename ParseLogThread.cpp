#include "ParseLogThread.h"
#include <QFile>
#include <stdio.h>
#include <stdlib.h>

ParseLogThread::ParseLogThread(QObject *parent /*= nullptr*/)
    : QThread(parent)
    , m_requestCount(0)
    , m_stoped(false)
    , m_filePath("")
    , m_keywords({})
    , m_lastParseIndex(0)
{

}

ParseLogThread::~ParseLogThread()
{

}

void ParseLogThread::run()
{
    emit sigStart();

    m_stoped = false;

    // 是否是增量查询结果
    bool isIncrementalParse = false;

    // 已检索行数
    int queryLineCount = 0;

    while(!m_stoped)
    {
        if(m_requestCount <= 0)
        {
            continue;
        }

        // 开始解析文件
        QFile file(m_filePath);
        if(file.open(QIODevice::ReadOnly))
        {
            uchar* fPtr = file.map(m_lastParseIndex, file.size() - m_lastParseIndex);
            if(fPtr)
            {
                // 更新 m_lastParseIndex
                m_lastParseIndex = file.size();

                // 按 '\n' 分割解析日志
                char* s = _strdup((char*)fPtr);
                char* substr;
                char* next = NULL;
                while (substr = strtok_s(s,"\n", &next))
                {
                    s = next;

                    ++queryLineCount;

                    QString qstr(substr);
                    if(qstr.isEmpty())
                    {
                        continue;
                    }

                    for(const auto& keyword : m_keywords)
                    {
                        // 去除最后的换行符
                        qstr = qstr.trimmed();

                        int pos = qstr.indexOf(keyword);
                        if(pos >= 0)
                        {
                            emit sigParsedContent(isIncrementalParse, qstr, qstr.mid(pos));

                            if(false == isIncrementalParse)
                            {
                                isIncrementalParse = true;
                            }

                            // 有一个关键字匹配就跳出关键字循环
                            break;
                        }
                    }
                }

                if(s)
                {
                    /*free(s);*/  // 存在内存泄露
                }

                file.unmap(fPtr);
            }
            else
            {
                emit sigMapFileFailed();

                // 维护 m_requestCount
                --m_requestCount;

                continue;
            }

            // 关闭文件
            file.close();

            emit sigParseFinished(queryLineCount);
        }
        else
        {
            emit sigOpenFileFailed();

            // 维护 m_requestCount
            --m_requestCount;

            continue;
        }

        // 维护 m_requestCount
        --m_requestCount;
    }

    m_requestCount = 0;
    m_lastParseIndex = 0;

    emit sigStop();
}
