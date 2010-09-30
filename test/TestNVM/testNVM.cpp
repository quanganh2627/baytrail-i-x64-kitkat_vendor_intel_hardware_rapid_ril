//
// Use this function to test RapidRIL's NVM functionality.
// A good place to call this function is at the end of RIL_Init() in rildmain.cpp.
//
void testNVM(void)
{
    int  iLastVal     = -1;
    int  iOrigVal     = -2;
    char szLastCSQ[]  = "LastCSQ";
    char szNewStuff[] = "NewStuff";
    char szNewItem1[] = "NewItem1";
    char szNewItem2[] = "NewItem2";
    char szSomeText[] = "SomeText";
    char szConfText[32];

    CRepository repository;


    RIL_LOG_TRACE("testNVM() - START\r\n");

    // Read existing value
    if (!repository.Read(g_szGroupLastValues, g_szLastCLIP, iOrigVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not read Last CLIP.\r\n");
        goto Error;
    }

    // Change existing value
    iLastVal = ((0 == iOrigVal) ? 1 : 0);
    if (!repository.Write(g_szGroupLastValues, g_szLastCLIP, iLastVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not modify Last CLIP.\r\n");
        goto Error;
    }

    // Confirm existing value
    iLastVal = -1;
    if (!repository.Read(g_szGroupLastValues, g_szLastCLIP, iLastVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not confirm modified Last CLIP.\r\n");
        goto Error;
    }

    if (iLastVal != iOrigVal)
    {
        RIL_LOG_TRACE("testNVM() - Last CLIP: Modify SUCCEEDED\r\n");
    }
    else
    {
        RIL_LOG_TRACE("testNVM() - Last CLIP: Modify FAILED (actual value %d should be %d)\r\n", iLastVal, iOrigVal);
    }

    // Restore existing value
    if (!repository.Write(g_szGroupLastValues, g_szLastCLIP, iOrigVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not restore Last CLIP.\r\n");
        goto Error;
    }

    // Confirm restore value
    iLastVal = -1;
    if (!repository.Read(g_szGroupLastValues, g_szLastCLIP, iLastVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not confirm restored Last CLIP.\r\n");
        goto Error;
    }

    if (iLastVal == iOrigVal)
    {
        RIL_LOG_TRACE("testNVM() - Last CLIP: Restore SUCCEEDED\r\n");
    }
    else
    {
        RIL_LOG_TRACE("testNVM() - Last CLIP: Restore FAILED (actual value %d should be %d)\r\n", iLastVal, iOrigVal);
    }

    ///////////////////////////////////////////////////////////////////////

    iOrigVal = 50;
    iLastVal = -1;

    // Add new value to existing group
    if (!repository.Write(g_szGroupLastValues, szLastCSQ, iOrigVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not add Last CSQ.\r\n");
        goto Error;
    }

    // Confirm new value
    if (!repository.Read(g_szGroupLastValues, szLastCSQ, iLastVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not confirm added Last CSQ.\r\n");
        goto Error;
    }

    if (iLastVal == iOrigVal)
    {
        RIL_LOG_TRACE("testNVM() - Last CSQ: Add SUCCEEDED\r\n");
    }
    else
    {
        RIL_LOG_TRACE("testNVM() - Last CSQ: Add FAILED (actual value %d should be %d)\r\n", iLastVal, iOrigVal);
    }

    ///////////////////////////////////////////////////////////////////////

    iOrigVal = 13;
    iLastVal = -1;

    // Add new value to new group
    if (!repository.Write(szNewStuff, szNewItem1, iOrigVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not add New Item 1 to New Group.\r\n");
        goto Error;
    }

    // Confirm new value
    if (!repository.Read(szNewStuff, szNewItem1, iLastVal))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not confirm added New Item 1.\r\n");
        goto Error;
    }

    if (iLastVal == iOrigVal)
    {
        RIL_LOG_TRACE("testNVM() - New Item 1: Add SUCCEEDED\r\n");
    }
    else
    {
        RIL_LOG_TRACE("testNVM() - New Item 1: Add FAILED (actual value %d should be %d)\r\n", iLastVal, iOrigVal);
    }

    // Add new string value to new group
    if (!repository.Write(szNewStuff, szNewItem2, szSomeText))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not add New Item 2 to New Group.\r\n");
        goto Error;
    }

    // Confirm new value
    if (!repository.Read(szNewStuff, szNewItem2, szConfText, 32))
    {
        RIL_LOG_TRACE("testNVM() - ERROR: Could not confirm added New Item 2.\r\n");
        goto Error;
    }

    if (0 == strcmp(szSomeText, szConfText))
    {
        RIL_LOG_TRACE("testNVM() - New Item 2: Add SUCCEEDED\r\n");
    }
    else
    {
        RIL_LOG_TRACE("testNVM() - New Item 2: Add FAILED (actual value \"%s\" should be \"%s\")\r\n", szConfText, szSomeText);
    }

Error:
    RIL_LOG_TRACE("testNVM() - END\r\n");
}

