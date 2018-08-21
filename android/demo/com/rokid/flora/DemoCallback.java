package com.rokid.flora;

import android.util.Log;

public class DemoCallback implements Client.Callback {
	public DemoCallback(String id) {
		clientId = id;
	}

	public void receivedPost(String name, Caps msg, int type) {
		try {
			int i = msg.readInteger();
			Log.i(Demo.TAG, "client " + clientId + " received post msg " + name + ", " + i);
		} catch (CapsException e) {
		}
	}

	public Client.Callback.Reply receivedGet(String name, Caps msg) {
		try {
			String s = msg.readString();
			Log.i(Demo.TAG, "client " + clientId + " received get msg " + name + ", " + s);
		} catch (CapsException e) {
		}
		Client.Callback.Reply rep = new Client.Callback.Reply();
		rep.retCode = Client.SUCCESS;
		rep.msg = new Caps();
		rep.msg.writeString("world");
		return rep;
	}

	public void disconnected() {
	}

	private String clientId;
}
