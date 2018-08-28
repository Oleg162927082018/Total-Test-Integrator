#ifndef DBMANAGER_H
#define DBMANAGER_H

#include "Common/itestcasetemplate.h"

#include "test-case.h"
#include "test-case-cache.h"
#include "test-status.h"
#include "run-description.h"
#include "test-result.h"
#include "tag-collection.h"

class DBManager
{
public:
    // Full file name creation tools
    static QString GetRunName(int runNum);
    static QString GetOutFolder(QString testCaseFullFileName, QString testName, QString RunName);
    static QString GetResultFullFileName(QString testCaseFullFileName, QString RunName, QString testName, QString ext);
    static QString GetStatusFullFileName(QString testCaseFullFileName, QString testName, QString ext);

    // Test-case
    static void SaveTestCase(QString testCaseFullFileName, TestCase *tc);
    static TestCase *GetTestCase(QString testCaseFullFileName);

    // Test-case cache
    static void SaveTestCaseCache(QString testCaseFullFileName, QMap<QString, QDomDocument *> *testList);
    static TestCaseFolder *GetTestCaseCache(QString testCaseFullFileName);
    static TestCaseFolder *GetCacheFolder(QString statusFullFolderName, QString relativeSubFolderName);

    // Test status
    static void SaveTestStatus(QString testCaseFullFileName, QString testRelativeName, TestStatus *ts);
    static TestStatus *GetTestStatus(QString testCaseFullFileName, QString testRelativeName);

    // Test-plan
    static void SaveTestingPlan(QString planFullFileName, QList<TestCaseFolder *> *cache);
    static QDomDocument *GetTestingPlan(QString planFullFileName);

    // Run description
    static void SaveRunDescription(QString testCaseFullFileName, QString runName, RunDescription *rd);
    static RunDescription *GetRunDescription(QString testCaseFullFileName, QString runName);

    // Run result
    static void SaveTestResult(QString testCaseFullFileName, QString runName, QString relativeTestName, TestResult *tr);
    static void SaveTestConsoleOutput(QString testCaseFullFileName, QString runName, QString relativeTestName, QStringList *consoleOutput);
    static TestResult *GetTestResult(QString testCaseFullFileName, QString relativeTestName, QString runName);
    static QByteArray GetConsoleLog(QString testCaseFullFileName, QString relativeTestName, QString runName);

    //Other
    static QVector<int> *GetRunList(QString testCaseFullFileName);
    static QVector<int> *GetOutList(QString testCaseFullFileName, QString relativeTestName);

    //Tags
    static void SaveTagCollection(TagItem *tagCollection);
    static void SaveTagFolderElements(QDomElement &folderElement, TagItem *tagItem);
    static TagItem *GetTagCollection(QString fullFileName);
    static void LoadTagFolderElements(QDomElement &folderElement, TagItem *tagFolder);
    static void DeleteTagCollection(QString fullFileName);
};

#endif // DBMANAGER_H