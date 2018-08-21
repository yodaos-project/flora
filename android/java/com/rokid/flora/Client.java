package com.rokid.flora;

import java.util.List;
import java.util.Iterator;

public class Client {
	public Client() {
	}

	public void finalize() {
		destroy();
	}

	public int connect(String uri, Callback cb, int msgBufSize) {
		if (uri == null)
			return ERROR_INVALID_PARAM;
		int r = native_connect(uri, msgBufSize);
		if (r == SUCCESS)
			callback = cb;
		return r;
	}

	public void destroy() {
		native_destroy(native_handle);
		native_handle = 0;
	}

	public int subscribe(String name, int type) {
		if (native_handle == 0)
			return ERROR_CONNECTION;
		if (name == null)
			return ERROR_INVALID_PARAM;
		return native_subscribe(native_handle, name, type);
	}

	public int unsubscribe(String name, int type) {
		if (native_handle == 0)
			return ERROR_CONNECTION;
		if (name == null)
			return ERROR_INVALID_PARAM;
		return native_unsubscribe(native_handle, name, type);
	}

	public int post(String name, Caps msg, int type) {
		if (native_handle == 0)
			return ERROR_CONNECTION;
		if (name == null)
			return ERROR_INVALID_PARAM;
		return native_post(native_handle, name,
				msg == null ? 0 : msg.native_handle, type);
	}

	public int get(String name, Caps msg, List<Reply> replys, int timeout) {
		if (native_handle == 0)
			return ERROR_CONNECTION;
		if (name == null || replys == null || timeout < 0)
			return ERROR_INVALID_PARAM;
		return native_get(native_handle, name,
				msg == null ? 0 : msg.native_handle, replys, timeout);
	}

	public void destroyReplys(List<Reply> replys) {
		if (replys == null)
			return;
		Iterator<Reply> it = replys.iterator();
		Reply reply;
		while (it.hasNext()) {
			reply = it.next();
			if (reply != null)
				reply.msg.destroy();
		}
	}

	public static interface Callback {
		public void receivedPost(String name, Caps msg, int type);
		public Reply receivedGet(String name, Caps msg);
		public void disconnected();

		public static class Reply {
			public int retCode;
			public Caps msg;
		}
	}

	public static class Reply {
		public int retCode;
		public Caps msg;
		public String extra;
	}

	private void nativePostCallback(String name, long msg, int type) {
		Caps msgcaps = new Caps(msg);
		if (callback != null)
			callback.receivedPost(name, msgcaps, type);
		msgcaps.destroy();
	}

	private int nativeGetCallback(String name, long msg, long reply) {
		Caps msgcaps = new Caps(msg);
		if (callback != null) {
			Callback.Reply rep = callback.receivedGet(name, msgcaps);
			msgcaps.destroy();
			if (rep != null) {
				if (rep.msg != null) {
					native_set_pointer(rep.msg.native_handle, reply);
					rep.msg.destroy();
				}
				return rep.retCode;
			}
		}
		msgcaps.destroy();
		return SUCCESS;
	}

	private void nativeDisconnectCallback() {
		if (callback != null)
			callback.disconnected();
	}

	private static native void native_init();

	private native int native_connect(String uri, int bufsize);

	private native int native_subscribe(long h, String name, int type);

	private native int native_unsubscribe(long h, String name, int type);

	private native int native_post(long h, String name, long msg, int type);

	private native int native_get(long h, String name, long msg, List<Reply> replys, int timeout);

	private native void native_set_pointer(long src, long dst);

	private native void native_destroy(long h);

	private Callback callback;

	private long native_handle = 0;

	public static final int SUCCESS = 0;
	public static final int ERROR_AUTH = -1;
	public static final int ERROR_INVALID_PARAM = -2;
	public static final int ERROR_CONNECTION = -3;
	public static final int ERROR_TIMEOUT = -4;
	// 'get'目标事件不存在
	public static final int ERROR_NO_TARGET = -5;

	static {
		System.loadLibrary("flora-cli.jni");
		native_init();
	}
}
