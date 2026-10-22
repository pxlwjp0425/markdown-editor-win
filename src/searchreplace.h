#ifndef SEARCHREPLACE_H
#define SEARCHREPLACE_H

#include <QDialog>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

class SearchReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchReplaceDialog(QWidget *parent = nullptr);
    ~SearchReplaceDialog();

    void setEditor(QPlainTextEdit *editor);

public slots:
    void showReplace();

private slots:
    void findNext();
    void replaceNext();
    void replaceAll();

private:
    QPlainTextEdit *m_editor;
    
    // 搜索
    QLineEdit *m_searchEdit;
    QPushButton *m_findButton;
    QCheckBox *m_caseSensitiveCheck;
    QCheckBox *m_wholeWordsCheck;
    QCheckBox *m_regexCheck;
    
    // 替换
    QLineEdit *m_replaceEdit;
    QPushButton *m_replaceButton;
    QPushButton *m_replaceAllButton;
    
    QLabel *m_statusLabel;
};

#endif // SEARCHREPLACE_H