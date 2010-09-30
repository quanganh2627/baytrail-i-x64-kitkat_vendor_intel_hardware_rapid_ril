#include <stdio.h>
#include "sync_ops.h"
#include "thread_ops.h"
#include <unistd.h>

void scenario_1();
void scenario_2();
void scenario_3();
void scenario_4();
void scenario_5();
void scenario_6();
void scenario_7();
void scenario_8();
void scenario_9();

int main()
{
    scenario_1();
    scenario_2();
    scenario_3();
    scenario_4();
    scenario_5();
    scenario_6();
    scenario_7();
    scenario_8();
    scenario_9();

    return 0;
}

// Scenario 1 : Wait on a single manual event
// Verifies: WAIT_FOREVER timeout, manual reset, timeout
void * scenario_1_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    printf("AA Waiting on event AA\r\n");

    CEvent* pEvent = (CEvent*) pVoid;
    UINT32 dwRet = pEvent->Wait(WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("AA Thread A has detected the set event AA\r\n");
    }
    else
    {
        printf("AA Thread A did not detect the event! AA\r\n");
        return NULL;
    }

    printf("AA Verify that event is still set AA\r\n");

    dwRet = pEvent->Wait(WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("AA Thread A has detected the set event AA\r\n");
    }
    else
    {
        printf("AA Thread A did not detect the event! AA\r\n");
        return NULL;
    }

    printf("AA Clear event and wait 10 seconds, should timeout AA\r\n");
    pEvent->Reset();

    dwRet = pEvent->Wait(10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Thread A timed out waiting on event as expected AA\r\n");
    }
    else
    {
        printf("AA Thread A found an unexpected result to the wait: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_1_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");
    CEvent* pEvent = (CEvent*) pVoid;

    printf("BB Thread B is sleeping for 10 seconds BBS\r\n");
    sleep(10);

    printf("BB Thread B is setting the event BB\r\n");
    pEvent->Signal();

    printf("BB Thread B has exited BB\r\n");
}

void scenario_1()
{
    printf("Start scenario 1 : Test single manual event\r\n");

    printf("-- Create Event --\r\n");
    CEvent * pEvent = new CEvent(NULL, TRUE);
    if (!pEvent)
    {
        printf("!! Failed to create event !!\r\n");
        return;
    }

    CThread * pThreadA = new CThread(scenario_1_thread_proc_a, (void*)pEvent, THREAD_FLAGS_JOINABLE, NULL);
    CThread * pThreadB = new CThread(scenario_1_thread_proc_b, (void*)pEvent, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);

    printf("-- Detected Thread A has exited, test complete --\r\n");
    delete pEvent;
    delete pThreadA;
    delete pThreadB;
}

// Scenario 2 : Wait on a single auto-reset event
// Verifies: Auto reset, timeout
void * scenario_2_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    printf("AA Waiting on event AA\r\n");

    CEvent* pEvent = (CEvent*) pVoid;
    UINT32 dwRet = pEvent->Wait(WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("AA Thread A has detected the set event AA\r\n");
    }
    else
    {
        printf("AA Thread A did not detect the event! AA\r\n");
        return NULL;
    }

    printf("AA Verify that event is not still set AA\r\n");

    dwRet = pEvent->Wait(10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Thread A has timed out as expected AA\r\n");
    }
    else
    {
        printf("AA Thread A got an unexpected result: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_2_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");
    CEvent* pEvent = (CEvent*) pVoid;

    printf("BB Thread B is sleeping for 10 seconds BB\r\n");
    sleep(10);

    printf("BB Thread B is setting the event BB\r\n");
    pEvent->Signal();

    printf("BB Thread B has exited BB\r\n");
}

void scenario_2()
{
    printf("Start scenario 2 : Test single auto-reset event\r\n");

    printf("-- Create Event --\r\n");
    CEvent * pEvent = new CEvent(NULL, FALSE);
    if (!pEvent)
    {
        printf("!! Failed to create event !!\r\n");
        return;
    }

    CThread * pThreadA = new CThread(scenario_2_thread_proc_a, (void*)pEvent, THREAD_FLAGS_JOINABLE, NULL);
    CThread * pThreadB = new CThread(scenario_2_thread_proc_b, (void*)pEvent, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);

    printf("-- Detected Thread A has exited, test complete --\r\n");
    delete pEvent;
    delete pThreadA;
    delete pThreadB;
}

// Scenario 3 : Wait on multiple manual reset events
// Verifies: Multiple manual reset, timeout
void * scenario_3_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    printf("AA Waiting on events AA\r\n");

    CEvent** rgpEvents = (CEvent**) pVoid;

    UINT32 dwRet = WaitForAnyEvent(2, rgpEvents, WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED + 1 == dwRet)
    {
        printf("AA Event 1 was signaled as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected response: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Verify event is still set AA\r\n");

    dwRet = WaitForAnyEvent(2, rgpEvents, WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED + 1 == dwRet)
    {
        printf("AA Event 1 was signaled as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected response: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Reset event and try waiting again, expect timeout AA\r\n");

    rgpEvents[1]->Reset();
    dwRet = WaitForAnyEvent(2, rgpEvents, 10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Wait timed out as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected response: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_3_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");
    CEvent** rgpEvents = (CEvent**) pVoid;

    printf("BB Thread B is sleeping for 10 seconds BB\r\n");
    sleep(10);

    printf("BB Thread B is setting the event BB\r\n");
    rgpEvents[1]->Signal();

    printf("BB Thread B has exited BB\r\n");
}

void scenario_3()
{
    CEvent * pEventA = NULL;
    CEvent * pEventB = NULL;
    CEvent ** rgpEvents = NULL;
    CThread * pThreadA = NULL;
    CThread * pThreadB = NULL;

    printf("Start scenario 3 : Test multiple manual reset events\r\n");
    
    printf("-- Create Event A --\r\n");
    pEventA = new CEvent(NULL, TRUE);
    if (!pEventA)
    {
        printf("!! Failed to create Event A!!\r\n");
        goto Error;
    }

    printf("-- Create Event B --\r\n");
    pEventB = new CEvent(NULL, TRUE);
    if (!pEventA)
    {
        printf("!! Failed to create Event B!!\r\n");
        return;
    }

    rgpEvents = new CEvent*[2];
    rgpEvents[0] = pEventA;
    rgpEvents[1] = pEventB;

    pThreadA = new CThread(scenario_3_thread_proc_a, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadB = new CThread(scenario_3_thread_proc_b, (void*)rgpEvents, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);

    printf("-- Detected Thread A has exited, test complete --\r\n");

Error:
    delete pEventA;
    delete pEventB;
    delete rgpEvents;
    delete pThreadA;
    delete pThreadB;
}

// Scenario 4 : Wait on multiple auto reset events
// Verifies: Multiple auto reset, two events set at same time (before and after wait starts), timeout
void * scenario_4_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    printf("AA Waiting on events AA\r\n");

    CEvent** rgpEvents = (CEvent**) pVoid;

    UINT32 dwRet = WaitForAnyEvent(2, rgpEvents, WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED + 1 == dwRet)
    {
        printf("AA Event 1 was signaled as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected response: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Verify event is no longer set AA\r\n");

    dwRet = WaitForAnyEvent(2, rgpEvents, 5000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Timed out as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected response: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Wait for either event AA\r\n");

    dwRet = WaitForAnyEvent(2, rgpEvents, 10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Timed out during wait AA\r\n");
        return NULL;
    }
    else
    {
        printf("AA Event %d was set, wait again and verify different event signaled AA\r\n", dwRet);
    }

    UINT32 dwRet2 = WaitForAnyEvent(2, rgpEvents, 10000);

    if (WAIT_TIMEDOUT == dwRet2)
    {
        printf("AA Timed out during wait AA\r\n");
        return NULL;
    }
    else if (dwRet != dwRet2)
    {
        printf("AA Event %d was set AA\r\n", dwRet2);
    }
    else
    {
        printf("AA Repeat event found: 0x%X AA\r\n", dwRet2);
        return NULL;
    }

    printf("AA Verify both events were reset, should timeout on wait\r\n");

    dwRet = WaitForAnyEvent(2, rgpEvents, 10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Timed out as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected response: 0x%X\r\n", dwRet);
        return NULL;
    }

    printf("AA Sleep for 20 secs AA\r\n");
    sleep(20);

    printf("AA Wait for either event AA\r\n");

    dwRet = WaitForAnyEvent(2, rgpEvents, 10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Timed out during wait AA\r\n");
        return NULL;
    }
    else
    {
        printf("AA Event %d was set, wait again and verify different event signaled AA\r\n", dwRet);
    }

    dwRet2 = WaitForAnyEvent(2, rgpEvents, 10000);

    if (WAIT_TIMEDOUT == dwRet2)
    {
        printf("AA Timed out during wait AA\r\n");
        return NULL;
    }
    else if (dwRet != dwRet2)
    {
        printf("AA Event %d was set AA\r\n", dwRet2);
    }
    else
    {
        printf("AA Repeat event found: 0x%X AA\r\n", dwRet2);
        return NULL;
    }

    printf("AA Verify both events were reset, should timeout on wait\r\n");

    dwRet = WaitForAnyEvent(2, rgpEvents, 10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Timed out as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected response: 0x%X\r\n", dwRet);
        return NULL;
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_4_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");
    CEvent** rgpEvents = (CEvent**) pVoid;

    printf("BB Thread B is sleeping for 10 seconds BB\r\n");
    sleep(10);

    rgpEvents[1]->Signal();

    printf("BB Thread B is sleeping for 10 seconds BB\r\n");
    sleep(10);

    printf("BB Thread B is setting both events BB\r\n");
    rgpEvents[0]->Signal();
    rgpEvents[1]->Signal();

    printf("BB Thread B is sleeping for 15 seconds BB\r\n");
    sleep(15);

    printf("BB Thread B is setting both events BB\r\n");
    rgpEvents[0]->Signal();
    rgpEvents[1]->Signal();

    printf("BB Thread B has exited BB\r\n");
}

void scenario_4()
{
    CEvent * pEventA = NULL;
    CEvent * pEventB = NULL;
    CEvent ** rgpEvents = NULL;
    CThread * pThreadA = NULL;
    CThread * pThreadB = NULL;

    printf("Start scenario 4 : Test multiple auto reset events\r\n");
    
    printf("-- Create Event A --\r\n");
    pEventA = new CEvent();
    if (!pEventA)
    {
        printf("!! Failed to create Event A!!\r\n");
        goto Error;
    }

    printf("-- Create Event B --\r\n");
    pEventB = new CEvent();
    if (!pEventA)
    {
        printf("!! Failed to create Event B!!\r\n");
        return;
    }

    rgpEvents = new CEvent*[2];
    rgpEvents[0] = pEventA;
    rgpEvents[1] = pEventB;

    pThreadA = new CThread(scenario_4_thread_proc_a, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadB = new CThread(scenario_4_thread_proc_b, (void*)rgpEvents, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);

    printf("-- Detected Thread A has exited, test complete --\r\n");

Error:
    delete pEventA;
    delete pEventB;
    delete [] rgpEvents;
    delete pThreadA;
    delete pThreadB;
}

// Scenario 5 : Multiple threads waiting on single manual reset event
// Verifies: Multiple observers get updated correctly when signaled for manual reset event
void * scenario_5_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    printf("AA Waiting on event AA\r\n");

    CEvent* pEvent = (CEvent*) pVoid;
    UINT32 dwRet = pEvent->Wait(20000);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("AA Thread A has detected the set event AA\r\n");
    }
    else
    {
        printf("AA Thread A did not detect the event! AA\r\n");
        return NULL;
    }

    printf("AA Sleep for 5 seconds AA\r\n");
    sleep(5);

    printf("AA Verify that event is still set AA\r\n");

    dwRet = pEvent->Wait(20000);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("AA Thread A has detected the set event AA\r\n");
    }
    else
    {
        printf("AA Thread A did not detect the event! AA\r\n");
        return NULL;
    }

    printf("AA Clear event and wait 10 seconds, should timeout AA\r\n");
    pEvent->Reset();

    dwRet = pEvent->Wait(10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Thread A timed out waiting on event as expected AA\r\n");
    }
    else
    {
        printf("AA Thread A found an unexpected result to the wait: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_5_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");

    printf("BB Waiting on event BB\r\n");

    CEvent* pEvent = (CEvent*) pVoid;
    UINT32 dwRet = pEvent->Wait(WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("BB Thread B has detected the set event BB\r\n");
    }
    else
    {
        printf("BB Thread B did not detect the event! BB\r\n");
        return NULL;
    }

    printf("BB Thread B has exited BB\r\n");
}

void * scenario_5_thread_proc_c(void * pVoid)
{
    printf("CC Thread C has started CC\r\n");
    CEvent* pEvent = (CEvent*) pVoid;

    printf("CC Thread C is sleeping for 10 seconds CC\r\n");
    sleep(10);

    printf("CC Thread C is setting the event CC\r\n");
    pEvent->Signal();

    printf("CC Thread C has exited CC\r\n");
}

void scenario_5()
{
    CEvent * pEvent = NULL;
    CThread * pThreadA = NULL;
    CThread * pThreadB = NULL;
    CThread * pThreadC = NULL;

    printf("Start scenario 5 : Test multiple thread single manual reset event\r\n");
    
    printf("-- Create Event --\r\n");
    pEvent = new CEvent(NULL, TRUE);
    if (!pEvent)
    {
        printf("!! Failed to create Event !!\r\n");
        goto Error;
    }

    pThreadA = new CThread(scenario_5_thread_proc_a, (void*)pEvent, THREAD_FLAGS_JOINABLE, NULL);
    pThreadB = new CThread(scenario_5_thread_proc_b, (void*)pEvent, THREAD_FLAGS_JOINABLE, NULL);
    pThreadC = new CThread(scenario_5_thread_proc_c, (void*)pEvent, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread A has exited --\r\n");

    printf("-- Waiting for Thread B to finish --\r\n");
    pThreadB->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread B has exited --\r\n");

    printf("-- Test complete --\r\n");

Error:
    delete pEvent;
    delete pThreadA;
    delete pThreadB;
    delete pThreadC;
}

// Scenario 6 : Multiple threads waiting on single auto reset event
// Verifies: Multiple observers get updated correctly when signaled for auto reset event
void * scenario_6_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    printf("AA Waiting on event AA\r\n");

    CEvent* pEvent = (CEvent*) pVoid;
    UINT32 dwRet = pEvent->Wait(20000);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("AA Thread A has detected the set event AA\r\n");
    }
    else
    {
        printf("AA Thread A did not detect the event! AA\r\n");
        return NULL;
    }

    printf("AA Sleep for 5 seconds AA\r\n");
    sleep(5);

    printf("AA Verify that event is no longer set AA\r\n");

    dwRet = pEvent->Wait(10000);

    if (WAIT_TIMEDOUT == dwRet)
    {
        printf("AA Thread A has timed out as expected AA\r\n");
    }
    else
    {
        printf("AA Unexpected result: 0x%X AA\r\n", dwRet);
        return NULL;
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_6_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");

    printf("BB Waiting on event BB\r\n");

    CEvent* pEvent = (CEvent*) pVoid;
    UINT32 dwRet = pEvent->Wait(WAIT_FOREVER);

    if (WAIT_EVENT_0_SIGNALED == dwRet)
    {
        printf("BB Thread B has detected the set event BB\r\n");
    }
    else
    {
        printf("BB Thread B did not detect the event! BB\r\n");
        return NULL;
    }

    printf("BB Thread B has exited BB\r\n");
}

void * scenario_6_thread_proc_c(void * pVoid)
{
    printf("CC Thread C has started CC\r\n");
    CEvent* pEvent = (CEvent*) pVoid;

    printf("CC Thread C is sleeping for 10 seconds CC\r\n");
    sleep(10);

    printf("CC Thread C is setting the event CC\r\n");
    pEvent->Signal();

    printf("CC Thread C has exited CC\r\n");
}

void scenario_6()
{
    CEvent * pEvent = NULL;
    CThread * pThreadA = NULL;
    CThread * pThreadB = NULL;
    CThread * pThreadC = NULL;

    printf("Start scenario 6 : Test multiple thread single auto reset event\r\n");
    
    printf("-- Create Event --\r\n");
    pEvent = new CEvent();
    if (!pEvent)
    {
        printf("!! Failed to create Event !!\r\n");
        goto Error;
    }

    pThreadA = new CThread(scenario_6_thread_proc_a, (void*)pEvent, THREAD_FLAGS_JOINABLE, NULL);
    pThreadB = new CThread(scenario_6_thread_proc_b, (void*)pEvent, THREAD_FLAGS_JOINABLE, NULL);
    pThreadC = new CThread(scenario_6_thread_proc_c, (void*)pEvent, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread A has exited --\r\n");

    printf("-- Waiting for Thread B to finish --\r\n");
    pThreadB->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread B has exited --\r\n");

    printf("-- Test complete --\r\n");

Error:
    delete pEvent;
    delete pThreadA;
    delete pThreadB;
    delete pThreadC;
}

// Scenario 7 : Multiple threads waiting on multiple manual reset events
// Verifies: Multiple observers get updated correctly when signaled for manual reset events
void * scenario_7_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    UINT32 dwRet = 0;
    CEvent** rgpEvents = (CEvent**) pVoid;

    CEvent * rgpThreadAEvents[2];
    rgpThreadAEvents[0] = rgpEvents[0];
    rgpThreadAEvents[1] = rgpEvents[1];

    while (WAIT_TIMEDOUT != dwRet)
    {
        dwRet = WaitForAnyEvent(2, rgpThreadAEvents, 20000);

        switch(dwRet)
        {
            case WAIT_EVENT_0_SIGNALED:
                printf("AA Thread A detected Event A AA\r\n");
                break;

            case WAIT_EVENT_0_SIGNALED + 1:
                printf("AA Thread A detected Event B AA\r\n");
                break;

            default:
                printf("AA Thread A timed out! AA\r\n");
                break;
        }

        sleep(10);
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_7_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");

    UINT32 dwRet = 0;
    CEvent** rgpEvents = (CEvent**) pVoid;

    CEvent * rgpThreadBEvents[2];
    rgpThreadBEvents[0] = rgpEvents[1];
    rgpThreadBEvents[1] = rgpEvents[2];

    while (WAIT_TIMEDOUT != dwRet)
    {
        dwRet = WaitForAnyEvent(2, rgpThreadBEvents, 20000);

        switch(dwRet)
        {
            case WAIT_EVENT_0_SIGNALED:
                printf("BB Thread B detected Event B BB\r\n");
                break;

            case WAIT_EVENT_0_SIGNALED + 1:
                printf("BB Thread B detected Event C BB\r\n");
                break;

            default:
                printf("BB Thread B timed out! BB\r\n");
                break;
        }

        sleep(10);
    }

    printf("BB Thread B has exited BB\r\n");
}

void * scenario_7_thread_proc_c(void * pVoid)
{
    printf("CC Thread C has started CC\r\n");

    UINT32 dwRet = 0;
    CEvent** rgpEvents = (CEvent**) pVoid;

    CEvent * rgpThreadCEvents[2];
    rgpThreadCEvents[0] = rgpEvents[0];
    rgpThreadCEvents[1] = rgpEvents[2];

    while (WAIT_TIMEDOUT != dwRet)
    {
        dwRet = WaitForAnyEvent(2, rgpThreadCEvents, 20000);

        switch(dwRet)
        {
            case WAIT_EVENT_0_SIGNALED:
                printf("CC Thread C detected Event A CC\r\n");
                break;

            case WAIT_EVENT_0_SIGNALED + 1:
                printf("CC Thread C detected Event C CC\r\n");
                break;

            default:
                printf("CC Thread C timed out! CC\r\n");
                break;
        }

        sleep(10);
    }

    printf("CC Thread C has exited CC\r\n");
}

void * scenario_7_thread_proc_d(void * pVoid)
{
    printf("DD Thread D has started DD\r\n");

    CEvent** rgpEvents = (CEvent**) pVoid;

    printf("DD Thread D is sleeping for 5 seconds DD\r\n");
    sleep(5);

    printf("DD Thread D is setting event A DD\r\n");
    rgpEvents[0]->Signal();

    printf("DD Thread D is sleeping for 10 seconds DD\r\n");
    sleep(10);

    printf("DD Thread D is resetting event A DD\r\n");
    rgpEvents[0]->Reset();

    printf("DD Thread D is setting event B DD\r\n");
    rgpEvents[1]->Signal();

    printf("DD Thread D is sleeping for 10 seconds DD\r\n");
    sleep(10);

    printf("DD Thread D is resetting event B DD\r\n");
    rgpEvents[1]->Reset();

    printf("DD Thread D is setting event C DD\r\n");
    rgpEvents[2]->Signal();

    printf("DD Thread D is sleeping for 10 seconds DD\r\n");
    sleep(10);

    printf("DD Thread D is resetting event C DD\r\n");
    rgpEvents[2]->Reset();

    printf("DD Thread D has exited DD\r\n");
}

void scenario_7()
{
    CEvent * pEventA = NULL;
    CEvent * pEventB = NULL;
    CEvent * pEventC = NULL;
    CEvent ** rgpEvents = NULL;
    CThread * pThreadA = NULL;
    CThread * pThreadB = NULL;
    CThread * pThreadC = NULL;
    CThread * pThreadD = NULL;

    printf("Start scenario 7 : Test multiple thread multiple manual reset event\r\n");
    
    printf("-- Create Event A --\r\n");
    pEventA = new CEvent(NULL, TRUE);
    if (!pEventA)
    {
        printf("!! Failed to create Event A !!\r\n");
        goto Error;
    }

    printf("-- Create Event B --\r\n");
    pEventB = new CEvent(NULL, TRUE);
    if (!pEventB)
    {
        printf("!! Failed to create Event B !!\r\n");
        goto Error;
    }

    printf("-- Create Event C --\r\n");
    pEventC = new CEvent(NULL, TRUE);
    if (!pEventC)
    {
        printf("!! Failed to create Event C !!\r\n");
        goto Error;
    }

    rgpEvents = new CEvent*[3];
    rgpEvents[0] = pEventA;
    rgpEvents[1] = pEventB;
    rgpEvents[2] = pEventC;

    pThreadA = new CThread(scenario_7_thread_proc_a, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadB = new CThread(scenario_7_thread_proc_b, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadC = new CThread(scenario_7_thread_proc_c, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadD = new CThread(scenario_7_thread_proc_d, (void*)rgpEvents, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread A has exited --\r\n");

    printf("-- Waiting for Thread B to finish --\r\n");
    pThreadB->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread B has exited --\r\n");

    printf("-- Waiting for Thread C to finish --\r\n");
    pThreadC->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread C has exited --\r\n");

    printf("-- Test complete --\r\n");

 Error:
    delete pEventA;
    delete pEventB;
    delete pEventC;
    delete [] rgpEvents;
    delete pThreadA;
    delete pThreadB;
    delete pThreadC;
    delete pThreadD;
}

// Scenario 8 : Multiple threads waiting on multiple auto reset events
// Verifies: Multiple observers get updated correctly when signaled for auto reset events
void * scenario_8_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");

    UINT32 dwRet = 0;
    CEvent** rgpEvents = (CEvent**) pVoid;

    CEvent * rgpThreadAEvents[2];
    rgpThreadAEvents[0] = rgpEvents[0];
    rgpThreadAEvents[1] = rgpEvents[1];

    while (WAIT_TIMEDOUT != dwRet)
    {
        dwRet = WaitForAnyEvent(2, rgpThreadAEvents, 20000);

        switch(dwRet)
        {
            case WAIT_EVENT_0_SIGNALED:
                printf("AA Thread A detected Event A AA\r\n");
                break;

            case WAIT_EVENT_0_SIGNALED + 1:
                printf("AA Thread A detected Event B AA\r\n");
                break;

            default:
                printf("AA Thread A timed out! AA\r\n");
                break;
        }

        sleep(2);
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_8_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");

    UINT32 dwRet = 0;
    CEvent** rgpEvents = (CEvent**) pVoid;

    CEvent * rgpThreadBEvents[2];
    rgpThreadBEvents[0] = rgpEvents[1];
    rgpThreadBEvents[1] = rgpEvents[2];

    while (WAIT_TIMEDOUT != dwRet)
    {
        dwRet = WaitForAnyEvent(2, rgpThreadBEvents, 20000);

        switch(dwRet)
        {
            case WAIT_EVENT_0_SIGNALED:
                printf("BB Thread B detected Event B BB\r\n");
                break;

            case WAIT_EVENT_0_SIGNALED + 1:
                printf("BB Thread B detected Event C BB\r\n");
                break;

            default:
                printf("BB Thread B timed out! BB\r\n");
                break;
        }

        sleep(2);
    }

    printf("BB Thread B has exited BB\r\n");
}

void * scenario_8_thread_proc_c(void * pVoid)
{
    printf("CC Thread C has started CC\r\n");

    UINT32 dwRet = 0;
    CEvent** rgpEvents = (CEvent**) pVoid;

    CEvent * rgpThreadCEvents[2];
    rgpThreadCEvents[0] = rgpEvents[0];
    rgpThreadCEvents[1] = rgpEvents[2];

    while (WAIT_TIMEDOUT != dwRet)
    {
        dwRet = WaitForAnyEvent(2, rgpThreadCEvents, 20000);

        switch(dwRet)
        {
            case WAIT_EVENT_0_SIGNALED:
                printf("CC Thread C detected Event A CC\r\n");
                break;

            case WAIT_EVENT_0_SIGNALED + 1:
                printf("CC Thread C detected Event C CC\r\n");
                break;

            default:
                printf("CC Thread C timed out! CC\r\n");
                break;
        }

        sleep(2);
    }

    printf("CC Thread C has exited CC\r\n");
}

void * scenario_8_thread_proc_d(void * pVoid)
{
    printf("DD Thread D has started DD\r\n");

    CEvent** rgpEvents = (CEvent**) pVoid;

    printf("DD Thread D is sleeping for 5 seconds DD\r\n");
    sleep(5);

    printf("DD Thread D is setting event A DD\r\n");
    rgpEvents[0]->Signal();

    printf("DD Thread D is sleeping for 10 seconds DD\r\n");
    sleep(10);

    printf("DD Thread D is setting event B DD\r\n");
    rgpEvents[1]->Signal();

    printf("DD Thread D is sleeping for 10 seconds DD\r\n");
    sleep(10);

    printf("DD Thread D is setting event C DD\r\n");
    rgpEvents[2]->Signal();

    printf("DD Thread D is sleeping for 10 seconds DD\r\n");
    sleep(10);

    printf("DD Thread D has exited DD\r\n");
}

void scenario_8()
{
    CEvent * pEventA = NULL;
    CEvent * pEventB = NULL;
    CEvent * pEventC = NULL;
    CEvent ** rgpEvents = NULL;
    CThread * pThreadA = NULL;
    CThread * pThreadB = NULL;
    CThread * pThreadC = NULL;
    CThread * pThreadD = NULL;

    printf("Start scenario 8 : Test multiple thread multiple auto reset events\r\n");
    
    printf("-- Create Event A --\r\n");
    pEventA = new CEvent();
    if (!pEventA)
    {
        printf("!! Failed to create Event A !!\r\n");
        goto Error;
    }

    printf("-- Create Event B --\r\n");
    pEventB = new CEvent();
    if (!pEventB)
    {
        printf("!! Failed to create Event B !!\r\n");
        goto Error;
    }

    printf("-- Create Event C --\r\n");
    pEventC = new CEvent();
    if (!pEventC)
    {
        printf("!! Failed to create Event C !!\r\n");
        goto Error;
    }

    rgpEvents = new CEvent*[3];
    rgpEvents[0] = pEventA;
    rgpEvents[1] = pEventB;
    rgpEvents[2] = pEventC;

    pThreadA = new CThread(scenario_8_thread_proc_a, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadB = new CThread(scenario_8_thread_proc_b, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadC = new CThread(scenario_8_thread_proc_c, (void*)rgpEvents, THREAD_FLAGS_JOINABLE, NULL);
    pThreadD = new CThread(scenario_8_thread_proc_d, (void*)rgpEvents, THREAD_FLAGS_NONE, NULL);

    printf("-- Waiting for Thread A to finish --\r\n");
    pThreadA->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread A has exited --\r\n");

    printf("-- Waiting for Thread B to finish --\r\n");
    pThreadB->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread B has exited --\r\n");

    printf("-- Waiting for Thread C to finish --\r\n");
    pThreadC->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread C has exited --\r\n");

    printf("-- Test complete --\r\n");

 Error:
    delete pEventA;
    delete pEventB;
    delete pEventC;
    delete [] rgpEvents;
    delete pThreadA;
    delete pThreadB;
    delete pThreadC;
    delete pThreadD;
}

// Scenario 9 : Test priority threading, thread wait timeout, delete running thread
// Verifies: Priority set/get, priority effective, thread wait timeout, delete running thread
typedef struct sScneario9Data
{
    CEvent * pEvent;
    int * pnLocal;
} SCENARIO_9_DATA;

void * scenario_9_thread_proc_a(void * pVoid)
{
    printf("AA Thread A has started AA\r\n");
    SCENARIO_9_DATA* pData = (SCENARIO_9_DATA*)pVoid;

    int* pnLocal = pData->pnLocal;
    CEvent * pEvent = pData->pEvent;

    delete pData;

    while (1)
    {
        if (WAIT_TIMEDOUT == pEvent->Wait(WAIT_FOREVER))
            break;

        (*pnLocal)++;
    }

    printf("AA Thread A has exited AA\r\n");
}

void * scenario_9_thread_proc_b(void * pVoid)
{
    printf("BB Thread B has started BB\r\n");
    SCENARIO_9_DATA* pData = (SCENARIO_9_DATA*)pVoid;

    int* pnLocal = pData->pnLocal;
    CEvent * pEvent = pData->pEvent;

    delete pData;

    while (1)
    {
        if (WAIT_TIMEDOUT == pEvent->Wait(10000))
            break;

        (*pnLocal)++;
    }

    printf("BB Thread B has exited BB\r\n");
}

void * scenario_9_thread_proc_c(void * pVoid)
{
    printf("CC Thread C has started CC\r\n");
    CEvent* pEvent = (CEvent*) pVoid;
    UINT32 dwLoops = 60000000;

    while (dwLoops > 0)
    {
        //sleep(1);
        pEvent->Signal();
        dwLoops--;
    }

    printf("CC Thread C has exited CC\r\n");
}

void scenario_9()
{
    CThread * pThreadA = NULL;
    CThread * pThreadB = NULL;
    CThread * pThreadC = NULL;
    CEvent * pEvent = NULL;

    int nThreadA = 0;
    int nThreadB = 0;

    SCENARIO_9_DATA * pScenario9DataA = new SCENARIO_9_DATA;
    SCENARIO_9_DATA * pScenario9DataB = new SCENARIO_9_DATA;

    printf("Start scenario 9 : Test priority threading\r\n");

    pEvent = new CEvent();

    pScenario9DataA->pnLocal = &nThreadA;
    pScenario9DataA->pEvent = pEvent;

    pScenario9DataB->pnLocal = &nThreadB;
    pScenario9DataB->pEvent = pEvent;

    pThreadA = new CThread(scenario_9_thread_proc_a, (void*)pScenario9DataA, THREAD_FLAGS_JOINABLE, NULL);
    pThreadB = new CThread(scenario_9_thread_proc_b, (void*)pScenario9DataB, THREAD_FLAGS_JOINABLE, NULL);
    pThreadC = new CThread(scenario_9_thread_proc_c, (void*)pEvent, THREAD_FLAGS_JOINABLE, NULL);

    printf("-- Setting ThreadA to HIGHEST and ThreadB to LOWEST priority --\r\n");

    pThreadA->SetPriority(THREAD_PRIORITY_LEVEL_HIGHEST);
    pThreadB->SetPriority(THREAD_PRIORITY_LEVEL_LOWEST);

    UINT32 dwRet = pThreadA->GetPriority();

    if (THREAD_PRIORITY_LEVEL_HIGHEST != dwRet)
    {
        printf("-- ThreadA returned incorrect priority! 0x%X --\r\n", dwRet);
    }
    else
    {
        printf("-- ThreadA returned correct priority --\r\n");
    }

    dwRet = pThreadB->GetPriority();

    if (THREAD_PRIORITY_LEVEL_LOWEST != dwRet)
    {
        printf("-- ThreadB returned incorrect priority! 0x%X --\r\n", dwRet);
    }
    else
    {
        printf("-- ThreadB returned correct priority --\r\n");
    }

    while(1)
    {
        printf("-- State: %s   Lead: %d     ThreadA: %d      ThreadB: %d --\r\n",
            (nThreadA > nThreadB) ? "TRUE" : "FALSE", 
            (nThreadA > nThreadB) ? nThreadA - nThreadB : nThreadB - nThreadA, 
            nThreadA, 
            nThreadB);

        if (WAIT_TIMEDOUT != pThreadC->WaitForThread(1000))
            break;
    }

    printf("-- Waiting for Thread A to finish --\r\n");
    if (WAIT_TIMEDOUT == pThreadA->WaitForThread(10000))
        printf("-- Thread A has not finished (as expected, testing timeout and delete of running thread)! --\r\n");
    else
        printf("-- Detected Thread A has exited --\r\n");

    printf("-- Waiting for Thread B to finish --\r\n");
    pThreadB->WaitForThread(WAIT_FOREVER);
    printf("-- Detected Thread B has exited --\r\n");

 Error:
    delete pThreadA;
    delete pThreadB;
    printf("-- Test complete --\r\n");
}
