#include "searchreplace.h"
#include <QRegularExpression>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRegularExpression>

SearchReplaceDialog::SearchReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("查找和替换"));
    setMinimumWidth(400);
    
    m_editor = nullptr;
    
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 搜索区域
    QGroupBox *searchGroup = new QGroupBox(tr("搜索"), this);
    QVBoxLayout *searchLayout = new QVBoxLayout(searchGroup);
    
    QHBoxLayout *searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索内容..."));
    m_findButton = new QPushButton(tr("查找下一个"), this);
    connect(m_findButton, &QPushButton::clicked, this, &SearchReplaceDialog::findNext);
    searchRow->addWidget(m_searchEdit);
    searchRow->addWidget(m_findButton);
    searchLayout->addLayout(searchRow);
    
    // 搜索选项
    QHBoxLayout *optionsRow = new QHBoxLayout();
    m_caseSensitiveCheck = new QCheckBox(tr("区分大小写"), this);
    m_wholeWordsCheck = new QCheckBox(tr("全词匹配"), this);
    m_regexCheck = new QCheckBox(tr("正则表达式"), this);
    optionsRow->addWidget(m_caseSensitiveCheck);
    optionsRow->addWidget(m_wholeWordsCheck);
    optionsRow->addWidget(m_regexCheck);
    optionsRow->addStretch();
    searchLayout->addLayout(optionsRow);
    
    mainLayout->addWidget(searchGroup);
    
    // 替换区域
    QGroupBox *replaceGroup = new QGroupBox(tr("替换"), this);
    QVBoxLayout *replaceLayout = new QVBoxLayout(replaceGroup);
    
    QHBoxLayout *replaceRow = new QHBoxLayout();
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("替换为..."));
    m_replaceButton = new QPushButton(tr("替换"), this);
    connect(m_replaceButton, &QPushButton::clicked, this, &SearchReplaceDialog::replaceNext);
    m_replaceAllButton = new QPushButton(tr("全部替换"), this);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &SearchReplaceDialog::replaceAll);
    replaceRow->addWidget(m_replaceEdit);
    replaceRow->addWidget(m_replaceButton);
    replaceRow->addWidget(m_replaceAllButton);
    replaceLayout->addLayout(replaceRow);
    
    mainLayout->addWidget(replaceGroup);
    
    // 状态标签
    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);
    
    // 搜索框回车触发查找
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchReplaceDialog::findNext);
}

SearchReplaceDialog::~SearchReplaceDialog()
{
}

void SearchReplaceDialog::setEditor(QPlainTextEdit *editor)
{
    m_editor = editor;
}

void SearchReplaceDialog::showReplace()
{
    // 显示替换区域
    for (QObject *child : children()) {
        QGroupBox *group = qobject_cast<QGroupBox*>(child);
        if (group && group->title() == tr("替换")) {
            group->show();
            break;
        }
    }
}

void SearchReplaceDialog::findNext()
{
    if (!m_editor) return;
    
    QString searchText = m_searchEdit->text();
    if (searchText.isEmpty()) return;
    
    QTextDocument::FindFlags flags;
    if (m_caseSensitiveCheck->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    if (m_wholeWordsCheck->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    
    QTextCursor cursor = m_editor->textCursor();
    
    if (m_regexCheck->isChecked()) {
        // 正则表达式搜索
        QRegularExpression regex(searchText, 
            m_caseSensitiveCheck->isChecked() ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
        
        if (!regex.isValid()) {
            m_statusLabel->setText(tr("无效的正则表达式"));
            return;
        }
        
        QString text = m_editor->toPlainText();
        QRegularExpressionMatch match = regex.match(text, cursor.position());
        
        if (match.hasMatch()) {
            QTextCursor newCursor(m_editor->document());
            newCursor.setPosition(match.capturedStart());
            newCursor.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
            m_editor->setTextCursor(newCursor);
            m_statusLabel->setText(tr("找到"));
        } else {
            // 从头开始搜索
            match = regex.match(text, 0);
            if (match.hasMatch()) {
                QTextCursor newCursor(m_editor->document());
                newCursor.setPosition(match.capturedStart());
                newCursor.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
                m_editor->setTextCursor(newCursor);
                m_statusLabel->setText(tr("从开头找到"));
            } else {
                m_statusLabel->setText(tr("未找到"));
            }
        }
    } else {
        // 普通搜索
        if (!m_editor->find(searchText, flags)) {
            // 从头开始搜索
            cursor.setPosition(0);
            m_editor->setTextCursor(cursor);
            if (!m_editor->find(searchText, flags)) {
                m_statusLabel->setText(tr("未找到"));
            }
        } else {
            m_statusLabel->setText(tr("找到"));
        }
    }
}

void SearchReplaceDialog::replaceNext()
{
    if (!m_editor) return;
    
    QTextCursor cursor = m_editor->textCursor();
    
    if (cursor.hasSelection()) {
        cursor.insertText(m_replaceEdit->text());
    }
    
    findNext();
}

void SearchReplaceDialog::replaceAll()
{
    if (!m_editor) return;
    
    QString searchText = m_searchEdit->text();
    QString replaceText = m_replaceEdit->text();
    
    if (searchText.isEmpty()) return;
    
    QString text = m_editor->toPlainText();
    int count = 0;
    
    if (m_regexCheck->isChecked()) {
        QRegularExpression regex(searchText, 
            m_caseSensitiveCheck->isChecked() ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
        
        if (!regex.isValid()) {
            m_statusLabel->setText(tr("无效的正则表达式"));
            return;
        }
        
        // Count matches before replacement
        QRegularExpressionMatchIterator matchIterator = regex.globalMatch(text);
        count = 0;
        while (matchIterator.hasNext()) {
            matchIterator.next();
            count++;
        }
        
        text.replace(regex, replaceText);
    } else {
        Qt::CaseSensitivity cs = m_caseSensitiveCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        
        if (m_wholeWordsCheck->isChecked()) {
            QRegularExpression rx("\\b" + searchText + "\\b");
            
            count = text.count(rx);
            text.replace(rx, replaceText);
        } else {
            count = text.count(searchText, cs);
            text.replace(searchText, replaceText, cs);
        }
    }
    
    m_editor->setPlainText(text);
    m_statusLabel->setText(tr("替换了 %1 处").arg(count));
}