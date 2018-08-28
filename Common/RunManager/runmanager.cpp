#include "runmanager.h"
#include "Common/DBManager/dbmanager.h"
#include "Common/DllManager/dllmanager.h"

#include <QThreadPool>
#include <QDebug>
#include <QDir>
#include <QDateTime>

RunManager *RunManager::Handle = NULL;

RunManager::RunManager(QObject *parent) : QObject(parent)
{
    QThreadPool::globalInstance()->setMaxThreadCount(0);
    QThreadPool::globalInstance()->setExpiryTimeout(-1);
}

RunManager::~RunManager()
{

}

void RunManager::Init()
{
    Handle = new RunManager();
}

void RunManager::FreeResources()
{
    delete Handle;
}

void RunManager::AddPlan(QString planFullFileName, QString runDescription)
{
    PlanQueueItem *newPlan = new PlanQueueItem();

    newPlan->fullFileName = planFullFileName;
    newPlan->description = runDescription;
    newPlan->status = PlanStatus::Wating;

    Handle->planQueue.append(newPlan);
}

int RunManager::GetQueueLength()
{
    return Handle->planQueue.length();
}

PlanQueueItem *RunManager::GetPlan(int ind)
{
    return Handle->planQueue.at(ind);
}

void RunManager::RemovePlan(int ind)
{
    if(Handle->planQueue.at(ind)->status != PlanStatus::Prosessing)
    {
        if(Handle->processingPlan > ind) { (Handle->processingPlan)--; }

        //delete Handle->planQueue.at(ind);
        Handle->planQueue.removeAt(ind);
    }
}

void RunManager::MoveUp(int ind)
{
    if(ind > 0)
    {
        Handle->planQueue.move(ind, ind - 1);
    }
}

void RunManager::MoveDown(int ind)
{
    if(ind < (Handle->planQueue.length() - 1))
    {
        Handle->planQueue.move(ind, ind + 1);
    }
}

void RunManager::Start(int ind)
{
    int planCount = Handle->planQueue.length();

    if(planCount > 0)
    {
        if((ind < 0) || (ind >= planCount)) { Handle->processingPlan = 0; }
        else { Handle->processingPlan = ind; }

        QThreadPool::globalInstance()->setMaxThreadCount(1);
        Handle->onTestFinished(NULL);
    }
}

void RunManager::Stop()
{
    QThreadPool::globalInstance()->setMaxThreadCount(0);
    emit Handle->killTests();

    PlanQueueItem *plan = Handle->planQueue.at(Handle->processingPlan);

    plan->status = PlanStatus::Wating;

    int newLogLine = plan->log.length();
    plan->log.append("Stopped by user.");
    plan->log.append("");

    emit Handle->testCaseFinished(Handle->processingPlan);
    emit Handle->planFinished(Handle->processingPlan);
    emit Handle->allFinished();
    emit Handle->logChanged(Handle->processingPlan, newLogLine);
}

void RunManager::Pause()
{
    QThreadPool::globalInstance()->setMaxThreadCount(0);
    Handle->planQueue.at(Handle->processingPlan)->status = PlanStatus::Pause;
}

//-----------------------------Perform Run tests -------------------------------------

void RunManager::onTestFinished(TestInfo *info)
{
    //Save result of finished tests and delete Test Info
    if(info != NULL)
    {
        endTest(info, planQueue.at(processingPlan));
        emit testFinished(processingPlan);
        delete info->comparator;
        delete info;
        if(processingTestCount > 0) { processingTestCount--; }
    }
    else
    {
        beginQueue();
    }

    if(QThreadPool::globalInstance()->maxThreadCount() == 0) { return; } //Pause

    do
    {
        processingPlan = lookingForPlan(processingPlan);

        if(processingPlan == -1)
        {
            endQueue();
            return;
        }

        doPlan(planQueue.at(processingPlan));

    } while (processingTestCount == 0);
}

int RunManager::lookingForPlan(int planInd)
{
    if(planInd == -1) { planInd = 0; }

    int planCandidate;
    for(int i = 0; i < planQueue.count(); i++)
    {
        planCandidate = (planInd + i) % planQueue.count();
        if(planQueue.at(planCandidate)->status != PlanStatus::Finished) { return planCandidate; }
    }

    return -1;
}

void RunManager::doPlan(PlanQueueItem *plan)
{
    QThreadPool *pool = QThreadPool::globalInstance();

    if(plan->status == PlanStatus::Wating) { beginWatingPlan(plan); }

    if(plan->status == PlanStatus::Pause) { beginPausePlan(plan); }

    plan->status = PlanStatus::Prosessing;

testCaseVerifying:

    if(plan->processedTestCase.isNull())
    {
        if(processingTestCount == 0)
        {
            endPlan(plan);
            plan->status = PlanStatus::Finished;
        }
        return;
    }

    while(!plan->processedTest.isNull())
    {
        if(processingTestCount >= pool->maxThreadCount()) { return; }

        plan->processedTest = plan->processedTest.nextSiblingElement("test");
        if(!plan->processedTest.isNull()) { beginTest(plan); }
    }

    //if(pool->activeThreadCount() > 0) { return; }
    if(processingTestCount > 0) { return; }

    endTestCase(plan);
    beginNextTestCase(plan);
    goto testCaseVerifying;
}

void RunManager::beginWatingPlan(PlanQueueItem *plan)
{
    //Imposible but need control
    if(plan == NULL) { return; }

    plan->planXml = DBManager::GetTestingPlan(plan->fullFileName);

    QDomElement runElement = plan->planXml->firstChildElement("run");
    plan->processedTestCase = runElement.firstChildElement("test-case");
    plan->testCount = runElement.attribute("count", "0").toInt();
    plan->testFinished = 0;

    int newLogLine = plan->log.length();
    plan->log.append("Plan contained " + QString::number(plan->testCount) + " tests.");
    plan->log.append("started ...");
    plan->log.append("");

    emit planStarted(planQueue.indexOf(plan));

    if(plan->processedTestCase.isNull()) { return; }

    cacheTestCaseData(plan);

    plan->log.append("Test-Case: " + QFileInfo(plan->processedTestCaseFullFileName).fileName() + " started...");

    emit testCaseStarted(planQueue.indexOf(plan));

    plan->processedTest = plan->processedTestCase.firstChildElement("test-list").firstChildElement("test");

    if(plan->processedTest.isNull()) { return; }

    beginTest(plan);

    emit testStarted(planQueue.indexOf(plan));
    emit logChanged(planQueue.indexOf(plan), newLogLine);
}

void RunManager::beginPausePlan(PlanQueueItem *plan)
{
    //Imposible but need control
    if(plan == NULL) { return; }

    int newLogLine = plan->log.length();
    plan->log.append("Resumed...");

    emit planStarted(planQueue.indexOf(plan));
    emit logChanged(planQueue.indexOf(plan), newLogLine);
}

void RunManager::endPlan(PlanQueueItem *plan)
{
    if(plan->planXml != NULL) { delete plan->planXml; }

    int newLogLine = plan->log.length();
    plan->log.append("Plan finished");

    emit planFinished(planQueue.indexOf(plan));
    emit logChanged(planQueue.indexOf(plan), newLogLine);
}

void RunManager::beginNextTestCase(PlanQueueItem *plan)
{
    //Imposible but need control
    if((plan == NULL) || (plan->processedTestCase.isNull())) { return; }

    plan->processedTestCase = plan->processedTestCase.nextSiblingElement("test-case");

    if(plan->processedTestCase.isNull()) { return; }

    cacheTestCaseData(plan);

    int newLogLine = plan->log.length();
    plan->log.append("Test-case: " + QFileInfo(plan->processedTestCaseFullFileName).fileName() + " started...");

    emit testCaseStarted(planQueue.indexOf(plan));
    emit logChanged(planQueue.indexOf(plan), newLogLine);

    plan->processedTest = plan->processedTestCase.firstChildElement("test-list").firstChildElement("test");

    if(plan->processedTest.isNull()) { return; }

    beginTest(plan);

    emit testStarted(planQueue.indexOf(plan));
}

void RunManager::cacheTestCaseData(PlanQueueItem *plan)
{
    //Imposible but need control
    if((plan == NULL) || (plan->processedTestCase.isNull())) { return; }

    plan->processedTestCaseID = plan->processedTestCase.firstChildElement("ID").text();
    plan->processedTestCaseFullFileName = plan->processedTestCase.firstChildElement("path").text();
    QVector<int> *runList = DBManager::GetRunList(plan->processedTestCaseFullFileName);

    int newRunNum = 1;
    if(runList->count() > 0) { newRunNum = runList->last() + 1; }
    plan->processedTestCaseRunName = DBManager::GetRunName(newRunNum);

    plan->processedTestCaseWaitingTime = plan->processedTestCase.firstChildElement("waiting-sec").text().toInt();
    plan->processedTestCaseMaxThreads = plan->processedTestCase.firstChildElement("max-threads").text().toInt();
    plan->processedTestCaseCompressionLevel = plan->processedTestCase.firstChildElement("compression").text().toInt();
    plan->processedTestCaseIsSaveOutPut = plan->processedTestCase.firstChildElement("is-save-output").text().compare("true", Qt::CaseInsensitive) == 0;

    plan->processedTestCaseExtraParams = plan->processedTestCase.firstChildElement("extra");

    plan->processedTestCaseStart = QDateTime::currentDateTime();
    plan->processedTestCaseMinTreads = plan->processedTestCaseMaxThreads;
    plan->processedTestCaseMaxTreads = plan->processedTestCaseMaxThreads;
    plan->processedTestCaseStatistics.clear();

    QThreadPool::globalInstance()->setMaxThreadCount(plan->processedTestCaseMaxThreads);

    //Create description of the run
    RunDescription rd;
    rd.LocalDateTimeOfStart = plan->processedTestCaseStart;
    rd.TestPlanFullFileName = plan->fullFileName;
    rd.Comment = plan->description;
    DBManager::SaveRunDescription(plan->processedTestCaseFullFileName, plan->processedTestCaseRunName, &rd);
}

void RunManager::endTestCase(PlanQueueItem *plan)
{
    RunDescription *rd = DBManager::GetRunDescription(plan->processedTestCaseFullFileName, plan->processedTestCaseRunName);
    if(rd != NULL)
    {
        rd->WorkingTimeMs = QDateTime::currentDateTime().toMSecsSinceEpoch() - plan->processedTestCaseStart.toMSecsSinceEpoch();
        rd->MinUsedThreads = plan->processedTestCaseMinTreads;
        rd->MaxUsedThreads = plan->processedTestCaseMaxTreads;
        rd->Statistics = plan->processedTestCaseStatistics;
        DBManager::SaveRunDescription(plan->processedTestCaseFullFileName, plan->processedTestCaseRunName, rd);
        delete rd;
    }

    int newLogLine = plan->log.length();
    plan->log.append("Test-case: " + QFileInfo(plan->processedTestCaseFullFileName).fileName() + " finished");
    plan->log.append("with results:");
    for(int i = 0 ; i < plan->processedTestCaseStatistics.count(); i++)
    {
        QString key = plan->processedTestCaseStatistics.keys().at(i);
        plan->log.append(key + " : " + QString::number(plan->processedTestCaseStatistics.value(key)));
    }
    plan->log.append("");

    emit testCaseFinished(planQueue.indexOf(plan));
    emit logChanged(planQueue.indexOf(plan), newLogLine);
}

void RunManager::beginTest(PlanQueueItem *plan)
{
    //Imposible but need control
    if((plan == NULL) || (plan->processedTest.isNull())) { return; }

    //Prepare test info data
    TestInfo *tstInf = new TestInfo();
    tstInf->testName = plan->processedTest.attribute("name");
    tstInf->outputFullFolderName =
            DBManager::GetOutFolder(plan->processedTestCaseFullFileName,
                                    tstInf->testName, plan->processedTestCaseRunName);
    tstInf->waitingTime = plan->processedTestCaseWaitingTime;
    tstInf->compressionLevel = plan->processedTestCaseCompressionLevel;
    tstInf->isSaveOutput = plan->processedTestCaseIsSaveOutPut;
    tstInf->comparator = DLLManager::GetTestCaseComparator(plan->processedTestCaseID);

    //Get command line from extra params and test data
    QDomDocument extraParams;
    QDomNode extraParamNode = plan->processedTestCaseExtraParams.firstChild();
    while(!extraParamNode.isNull())
    {
        extraParams.appendChild(extraParams.importNode(extraParamNode, true));
        extraParamNode = extraParamNode.nextSibling();
    }

    QDomDocument testData;
    QDomNode testDataNode = plan->processedTest.firstChild();
    while(!testDataNode.isNull())
    {
        testData.appendChild(testData.importNode(testDataNode, true));
        testDataNode = testDataNode.nextSibling();
    }

    DLLManager::GetTestCaseRunCommand(plan->processedTestCaseID,
                              plan->processedTestCaseFullFileName,
                              extraParams, testData, tstInf->outputFullFolderName,
                              tstInf->exeCommand, tstInf->argumentList, tstInf->workingFullFolderName);


    //Prepare benchmark, previous and other results output folder names
    if(plan->processedTestCaseCompressionLevel > 0)
    {
        TestStatus *ts = DBManager::GetTestStatus(plan->processedTestCaseFullFileName, tstInf->testName);
        if((ts != NULL) && (ts->benchmarks.keys().count() > 0))
        {
            int lastBenchmarkRunNum = ts->benchmarks.keys().last();
            tstInf->testResult.benchmarkRunMark = DBManager::GetRunName(lastBenchmarkRunNum);
            tstInf->testResult.benchmarkOutMark = ts->benchmarks.value(lastBenchmarkRunNum).outMark;
            tstInf->testResult.benchmarkStatus = ts->benchmarks.value(lastBenchmarkRunNum).status;
            delete ts;

            tstInf->benchmarkOutputFullFolderName =
                    DBManager::GetOutFolder(plan->processedTestCaseFullFileName,
                                            tstInf->testName, tstInf->testResult.benchmarkOutMark);
        }
    }


    if(plan->processedTestCaseCompressionLevel > 1)
    {
        TestResult *tr =
                DBManager::GetTestResult(plan->processedTestCaseFullFileName, tstInf->testName,
                                                  DBManager::GetRunName(plan->processedTestCaseRunName.toInt() - 1));
        if(tr != NULL)
        {
            tstInf->previousOutputFullFolderName =
                    DBManager::GetOutFolder(plan->processedTestCaseFullFileName, tstInf->testName, tr->outMark);
            delete tr;
        }
        else
        {
            tstInf->previousOutputFullFolderName = "";
        }
    }

    QVector<int> *outLinks =
            DBManager::GetOutList(plan->processedTestCaseFullFileName, tstInf->testName);

    for(int i = 0; i < outLinks->count(); i++)
    {
        tstInf->otherOutputFullFolderNames.append(
                    DBManager::GetOutFolder(plan->processedTestCaseFullFileName, tstInf->testName,
                                            DBManager::GetRunName(outLinks->at(i))));
    }

    //Create test executor
    TestExecutor *test = new TestExecutor(tstInf);
    connect(test, &TestExecutor::finished, Handle, &RunManager::onTestFinished, Qt::QueuedConnection);
    connect(RunManager::Handle, &RunManager::killTests, test, &TestExecutor::doKill, Qt::DirectConnection);

    //Create output folder
    QDir().mkpath(tstInf->outputFullFolderName);

    //Start test
    processingTestCount++;
    QThreadPool::globalInstance()->start(test);
}

void RunManager::endTest(TestInfo *testInfo, PlanQueueItem *plan)
{
    plan->testFinished += 1;

    if(testInfo->isSaveOutput)
    {
        DBManager::SaveTestConsoleOutput(plan->processedTestCaseFullFileName,
                                         plan->processedTestCaseRunName,
                                         testInfo->testName, &(testInfo->consoleOutput));
    }

    if((testInfo->compressionLevel != 0) && (testInfo->testResult.benchmarkCompareResult == -1) && (testInfo->testResult.exitCode == 0))
    {
        //Run was success but benchmark not found, so current result set as the best benchmark
        int runNum = plan->processedTestCaseRunName.toInt();

        BenchmarkInfo benchmarkInfo;
        benchmarkInfo.status = "thebest";
        benchmarkInfo.outMark = testInfo->testResult.outMark;

        TestStatus *ts = DBManager::GetTestStatus(plan->processedTestCaseFullFileName, testInfo->testName);
        ts->benchmarks.insert(runNum, benchmarkInfo);
        DBManager::SaveTestStatus(plan->processedTestCaseFullFileName, testInfo->testName, ts);

    }

    DBManager::SaveTestResult(plan->processedTestCaseFullFileName, plan->processedTestCaseRunName,
                              testInfo->testName, &(testInfo->testResult));

    QString statisticsKey = testInfo->testResult.status + " " + testInfo->testResult.color.name();
    plan->processedTestCaseStatistics[statisticsKey] = plan->processedTestCaseStatistics[statisticsKey] + 1;
}

void RunManager::beginQueue()
{

}

void RunManager::endQueue()
{
    emit allFinished();
}