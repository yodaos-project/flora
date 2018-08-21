package com.rokid.flora;

import java.util.ArrayList;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

public class Demo extends Service {
	public void onCreate() {
		client1 = new Client();
		client2 = new Client();
	}

	public int onStartCommand(Intent intent, int flags, int startId) {
		String extra1 = "foo1";
		String extra2 = "foo2";
		client1.destroy();
		client1.connect(FLORA_SERVICE_URI + "#" + extra1, new DemoCallback(extra1), 0);
		client2.destroy();
		client2.connect(FLORA_SERVICE_URI + "#" + extra2, new DemoCallback(extra2), 0);
		client1.subscribe("foo2.post", MsgType.INSTANT);
		client2.subscribe("foo1.get", MsgType.REQUEST);
		Caps msg = new Caps();
		ArrayList<Client.Reply> replys = new ArrayList<Client.Reply>();
		msg.writeString("hello");
		client1.get("foo1.get", msg, replys, 0);
		msg.destroy();
		Log.i(TAG, "foo1.get reply size = " + replys.size());
		Client.Reply rep = replys.get(0);
		Log.i(TAG, "foo.get reply code = " + rep.retCode);
		Log.i(TAG, "foo.get reply extra = " + rep.extra);
		if (rep.retCode == Client.SUCCESS) {
			try {
				String s = rep.msg.readString();
				rep.msg.destroy();
				Log.i(TAG, "foo.get reply msg = " + s);
			} catch (CapsException e) {
			}
		}
		msg = new Caps();
		msg.writeInteger(1);
		client2.post("foo2.post", msg, MsgType.INSTANT);
		msg.destroy();
		return START_NOT_STICKY;
	}

	public IBinder onBind(Intent intent) {
		return null;
	}

	private Client client1;
	private Client client2;

	private static final String FLORA_SERVICE_URI = "tcp://localhost:2517/";

	public static final String TAG = "flora-demo.android";
}
