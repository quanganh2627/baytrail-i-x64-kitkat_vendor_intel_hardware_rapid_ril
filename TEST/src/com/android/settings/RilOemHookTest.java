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
import java.nio.ByteBuffer;




public class RilOemHookTest extends Activity
{
    private static final String LOG_TAG = "RILOemHookTestApp";

    private RadioButton mRadioButtonAPI1 = null;

    private RadioGroup mRadioGroupAPI = null;

    private Phone mPhone = null;

    private static final int EVENT_RIL_OEM_HOOK_RAW_COMPLETE = 1300;
    private static final int EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE = 1310;
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
            {
                // RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR
                //  AT+XDRV=5,9,<sensor_id>

                // data: <command id>, <sensor id>
                String[] data = { "162", "3" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api2:
            {
                // RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD
                //  AT+XDRV=5,14,<activate>,<sensor_id>,<min_threshold>,<max_threshold>

                // data: <command id>, <activate>, <sensor_id>, <minThersholdTemp>, <maxThersholdTemp>
                String[] data = { "163", "true 3 2300 2300" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api3:
            {
                // RIL_OEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY
                //  AT+XFDOR=<enable>,<delay_timer>
                ByteBuffer myBuf = ByteBuffer.allocate(12);
                myBuf.putInt(0xA4);  //  Command ID
                myBuf.putInt(0x01);  //  <enable>
                myBuf.putInt(0x00);  //  <delay_timer>

                oemhook = myBuf.array();

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_RAW_COMPLETE);
                mPhone.invokeOemRilRequestRaw(oemhook, msg);
            }
            break;

            case R.id.radio_api4:
            {
                // RIL_OEM_HOOK_STRING_GET_ATR
                // AT+XGATR

                // data: <command id>
                String[] data = { "165" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api5:
            {
                // RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV
                // AT+CGED=0

                // data: <command id>
                String[] data = { "166" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api6:
            {
                // RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND
                // AT+XCGEDPAGE=0

                // data: <command id>
                String[] data = { "167" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api100:
            {
                //  RIL_OEM_HOOK_RAW_SET_ACTIVE_SIM
                //  AT@nvm:fix_uicc.ext_mux_misc_config=<sim_id>
                //  AT@nvm:store_nvm_sync(fix_uicc)
                //
                //  RIL then triggers warm modem reset

                ByteBuffer myBuf = ByteBuffer.allocate(8);
                myBuf.putInt(0xB0);  //  Command ID
                myBuf.putInt(0x00);  //  <sim_id>

                oemhook = myBuf.array();

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_RAW_COMPLETE);
                mPhone.invokeOemRilRequestRaw(oemhook, msg);
            }
            break;

            case R.id.radio_api101:
            {
                //  RIL_OEM_HOOK_RAW_GET_ACTIVE_SIM
                //  AT@nvm:fix_uicc.ext_mux_misc_config?
                //
                //  Response:
                //  <sim_id>
                //  OK
                //
                //  <sim_id> = int

                ByteBuffer myBuf = ByteBuffer.allocate(4);
                myBuf.putInt(0xB1);  //  Command ID

                oemhook = myBuf.array();

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_RAW_COMPLETE);
                mPhone.invokeOemRilRequestRaw(oemhook, msg);
            }
            break;

            default:
                log("unknown button selected");
                break;
        }


//        Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_COMMAND_COMPLETE);
//        mPhone.invokeOemRilRequestRaw(oemhook, msg);

    }

    private void logRilOemHookRawResponse(AsyncResult ar)
    {
        log("received oem hook raw response");

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
            log("received NULL oem hook raw response");
            str += "received NULL oem hook raw response";
        }


        //  Display message box
        AlertDialog.Builder builder = new AlertDialog.Builder(this);

        builder.setMessage(str);
        builder.setPositiveButton("OK", null);

        AlertDialog alert = builder.create();
        alert.show();
    }

    private void logRilOemHookStringsResponse(AsyncResult ar)
    {
        log("received oem hook strings response");

        String str = new String("");


        if (ar.exception != null)
        {
            log("Exception:" + ar.exception);
            str += "Exception:" + ar.exception + "\n\n";
        }

        if (ar.result != null)
        {
            String[] oemResponse = (String[])ar.result;
            int size = oemResponse.length;

            log("oemResponse length=[" + Integer.toString(size) + "]");
            str += "oemResponse length=[" + Integer.toString(size) + "]" + "\n";

            if (size > 0)
            {
                for (int i=0; i<size; i++)
                {
                    log("oemResponse[" + Integer.toString(i) + "]=[" + oemResponse[i] + "]");
                    str += "oemResponse[" + Integer.toString(i) + "]=[" + oemResponse[i] + "]" + "\n";
                }
            }
        }
        else
        {
            log("received NULL oem hook strings response");
            str += "received NULL oem hook strings response";
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
                case EVENT_RIL_OEM_HOOK_RAW_COMPLETE:
                    ar = (AsyncResult) msg.obj;
                    logRilOemHookRawResponse(ar);
                    break;

                case EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE:
                    ar = (AsyncResult) msg.obj;
                    logRilOemHookStringsResponse(ar);
                    break;

                case EVENT_UNSOL_RIL_OEM_HOOK_RAW:
                    break;
            }
        }
    };

}
