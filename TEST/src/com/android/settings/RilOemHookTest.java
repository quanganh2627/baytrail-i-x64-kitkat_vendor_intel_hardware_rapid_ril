//
// Copyright 2011 Intrinsyc Software International, Inc.  All rights reserved.
//


package com.android.RILTest;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;
import android.os.Message;
import android.os.Handler;
import android.os.AsyncResult;
import android.util.Log;
import android.app.AlertDialog;




public class RilOemHookTest extends Activity
{
    private static final String LOG_TAG = "RILOemHookTestApp";

    private RadioButton mRadioButtonAPI1 = null;

    private RadioGroup mRadioGroupAPI = null;

    private Phone mPhone = null;

    private static final int EVENT_RIL_OEM_HOOK_COMMAND_COMPLETE = 1300;
    private static final int EVENT_UNSOL_RIL_OEM_HOOK_RAW = 500;

    @Override
    public void onCreate(Bundle icicle)
    {
        super.onCreate(icicle);
        setContentView(R.layout.riloemhook_layout);

        mRadioButtonAPI1 = (RadioButton) findViewById(R.id.radio_api1);

        mRadioGroupAPI = (RadioGroup) findViewById(R.id.radio_group_api);

        //  Initially turn on first button.
        mRadioButtonAPI1.toggle();

        //  Get our main phone object.
        mPhone = PhoneFactory.getDefaultPhone();

        //  Register for OEM raw notification.
        //mPhone.mCM.setOnUnsolOemHookRaw(mHandler, EVENT_UNSOL_RIL_OEM_HOOK_RAW, null);


    }


    @Override
    public void onPause()
    {
        super.onPause();

        log("onPause()");

        //  Unregister for OEM raw notification.
        //mPhone.mCM.unSetOnUnsolOemHookRaw(mHandler);
    }

    @Override
    public void onResume()
    {
        super.onResume();

        log("onResume()");

        //  Register for OEM raw notification.
        //mPhone.mCM.setOnUnsolOemHookRaw(mHandler, EVENT_UNSOL_RIL_OEM_HOOK_RAW, null);
    }

    public void onRun(View view)
    {
        //  Get the checked button
        int idButtonChecked = mRadioGroupAPI.getCheckedRadioButtonId();

        byte[] oemhook = null;

        switch(idButtonChecked)
        {
            case R.id.radio_api1:
                // RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
                //  AT+XFDOR=1
                oemhook = new byte[1];
                oemhook[0] = (byte)0xBB;  // Command ID

                break;

            case R.id.radio_api2:
                // RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
                //  AT+XFDORT=<timer_value>

                oemhook = new byte[8];
                oemhook[0] = (byte)0xCC;  // Command ID
                oemhook[1] = (byte)0x00;  // padding
                oemhook[2] = (byte)0x00;
                oemhook[3] = (byte)0x00;

                oemhook[4] = (byte)0x00;  // TimerValue (int, low byte to high byte)
                oemhook[5] = (byte)0x00;  //  e.g. 0x0F 0x0A 0x07 0x03 = 0x03070A0F, 50792975.
                oemhook[6] = (byte)0x00;
                oemhook[7] = (byte)0x00;

                break;

            case R.id.radio_api3:
                //  RIL_OEM_HOOK_RAW_SET_ACTIVE_SIM
                //  AT+XSIM=sim_id

                oemhook = new byte[8];
                oemhook[0] = (byte)0xD0;  // Command ID
                oemhook[1] = (byte)0x00;  // padding
                oemhook[2] = (byte)0x00;
                oemhook[3] = (byte)0x00;

                oemhook[4] = (byte)0x00;  // Sim_id (int, low byte to high byte)
                oemhook[5] = (byte)0x00;  //  e.g. 0x0F 0x0A 0x07 0x03 = 0x03070A0F, 50792975.
                oemhook[6] = (byte)0x00;
                oemhook[7] = (byte)0x00;

                break;

            case R.id.radio_api4:
                //  RIL_OEM_HOOK_RAW_GET_ACTIVE_SIM
                //  AT+XSIM=?

                oemhook = new byte[1];
                oemhook[0] = (byte)0xD1;  // Command ID

                break;



            default:
                log("unknown button selected");
                break;
        }


        Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_COMMAND_COMPLETE);
        mPhone.invokeOemRilRequestRaw(oemhook, msg);



    }

    private void logRilOemHookResponse(AsyncResult ar)
    {
        log("received oem hook response");

        String str = new String("");


        if (ar.exception != null)
        {
            log("Exception:" + ar.exception);
            str += "Exception:" + ar.exception + "\n\n";
        }

        if (ar.result != null)
        {
            byte[] oemResponse = (byte[])ar.result;
            int size = oemResponse.length;

            log("oemResponse length=[" + Integer.toString(size) + "]");
            str += "oemResponse length=[" + Integer.toString(size) + "]" + "\n";

            if (size > 0)
            {
                for (int i=0; i<size; i++)
                {
                    byte myByte = oemResponse[i];
                    int myInt = (int)(myByte & 0x000000FF);
                    log("oemResponse[" + Integer.toString(i) + "]=[0x" + Integer.toString(myInt,16) + "]");
                    str += "oemResponse[" + Integer.toString(i) + "]=[0x" + Integer.toString(myInt,16) + "]" + "\n";
                }
            }
        }
        else
        {
            log("received NULL oem hook response");
            str += "received NULL oem hook response";
        }


        //  Display message box
        AlertDialog.Builder builder = new AlertDialog.Builder(this);

        builder.setMessage(str);
        builder.setPositiveButton("OK", null);

        AlertDialog alert = builder.create();
        alert.show();


    }

    private void log(String msg)
    {
        Log.d(LOG_TAG, "[RIL_HOOK_OEM_TESTAPP] " + msg);
    }

    private Handler mHandler = new Handler()
    {
        public void handleMessage(Message msg)
        {
            AsyncResult ar;
            switch (msg.what)
            {
                case EVENT_RIL_OEM_HOOK_COMMAND_COMPLETE:
                    ar = (AsyncResult) msg.obj;

                    logRilOemHookResponse(ar);
                    break;

                case EVENT_UNSOL_RIL_OEM_HOOK_RAW:

                    break;
            }
        }
    };

}
