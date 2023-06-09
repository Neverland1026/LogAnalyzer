﻿#include "ParseLogThread.h"
#include <QFile>
#include <stdio.h>
#include <stdlib.h>

ParseLogThread::ParseLogThread(QObject *parent /*= nullptr*/)
    : QThread(parent)
    , m_requestCount(0)
    , m_stoped(false)
    , m_filePath("")
    , m_keywords({})
    , m_caseSensitive(true)
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

    // 增量查询结果标记位
    bool isIncrementalParse = false;

    // 已检索行数
    int queryLineCount = 0;

    while(!m_stoped)
    {
        if(m_requestCount <= 0)
        {
            QThread::msleep(10);
            continue;
        }

        // 开始解析文件
        QFile file(m_filePath);
        if(file.open(QIODevice::ReadOnly))
        {
            emit sigFileExist();

            uchar* fPtr = file.map(m_lastParseIndex, file.size() - m_lastParseIndex);
            if(fPtr)
            {
                // 更新 m_lastParseIndex
                m_lastParseIndex = file.size();

                // 按 '\n' 分割解析日志
                char* b = _strdup((char*)fPtr);
                char* s = b;
                char* substr;
                char* next = NULL;
                while (substr = strtok_s(s, "\n", &next))
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

                        int pos = qstr.indexOf(keyword, 0, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
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

                file.unmap(fPtr);

                if(b)
                {
                    free(b);
                }
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

            // 本次解析完成
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

    // 还原变量
    m_requestCount = 0;
    m_lastParseIndex = 0;

    emit sigStop();
}
