package com.rokid.flora;

public class Caps {
	public Caps() {
	}

	Caps(long h) {
		native_handle = h;
	}

	public void finalize() {
		destroy();
	}

	public void destroy() {
		native_destroy(native_handle);
		native_handle = 0;
	}

	public int parse(byte[] data, int offset, int length) {
		throw new UnsupportedOperationException("Caps.parse");
		// return native_parse(data, offset, length);
	}

	public int parse(byte[] data) {
		if (data == null)
			return ERROR_INVALID_PARAM;
		return parse(data, 0, data.length);
	}

	public int serialize(byte[] data, int offset, int length) {
		throw new UnsupportedOperationException("Caps.serialize");
		// return native_serialize(data, offset, length);
	}

	public int writeInteger(int v) {
		if (native_handle == 0)
			native_handle = native_create();
		return native_write_integer(native_handle, v);
	}

	public int writeFloat(float v) {
		if (native_handle == 0)
			native_handle = native_create();
		return native_write_float(native_handle, v);
	}

	public int writeLong(long v) {
		if (native_handle == 0)
			native_handle = native_create();
		return native_write_long(native_handle, v);
	}

	public int writeDouble(double v) {
		if (native_handle == 0)
			native_handle = native_create();
		return native_write_double(native_handle, v);
	}

	public int writeString(String v) {
		if (native_handle == 0)
			native_handle = native_create();
		return native_write_string(native_handle, v);
	}

	public int writeBinary(byte[] v, int offset, int length) {
		if (native_handle == 0)
			native_handle = native_create();
		if (v != null && (offset < 0 || length < 0 || offset + length > v.length))
			return ERROR_INVALID_PARAM;
		return native_write_binary(native_handle, v, offset, length);
	}

	public int writeBinary(byte[] v) {
		if (v == null)
			return writeBinary(null, 0, 0);
		return writeBinary(v, 0, v.length);
	}

	public int writeCaps(Caps v) {
		if (native_handle == 0)
			native_handle = native_create();
		return native_write_caps(native_handle, v == null ? 0 : v.native_handle);
	}

	public int readInteger() throws CapsException {
		if (native_handle == 0)
			throw new CapsException(ERROR_INVALID_PARAM);
		IntegerValue v = new IntegerValue();
		int r = native_read(native_handle, v);
		if (r != SUCCESS)
			throw new CapsException(r);
		return v.value;
	}

	public float readFloat() throws CapsException {
		if (native_handle == 0)
			throw new CapsException(ERROR_INVALID_PARAM);
		FloatValue v = new FloatValue();
		int r = native_read(native_handle, v);
		if (r != SUCCESS)
			throw new CapsException(r);
		return v.value;
	}

	public long readLong() throws CapsException {
		if (native_handle == 0)
			throw new CapsException(ERROR_INVALID_PARAM);
		LongValue v = new LongValue();
		int r = native_read(native_handle, v);
		if (r != SUCCESS)
			throw new CapsException(r);
		return v.value;
	}

	public double readDouble() throws CapsException {
		if (native_handle == 0)
			throw new CapsException(ERROR_INVALID_PARAM);
		DoubleValue v = new DoubleValue();
		int r = native_read(native_handle, v);
		if (r != SUCCESS)
			throw new CapsException(r);
		return v.value;
	}

	public String readString() throws CapsException {
		if (native_handle == 0)
			throw new CapsException(ERROR_INVALID_PARAM);
		StringValue v = new StringValue();
		int r = native_read(native_handle, v);
		if (r != SUCCESS)
			throw new CapsException(r);
		return v.value;
	}

	public byte[] readBinary() throws CapsException {
		if (native_handle == 0)
			throw new CapsException(ERROR_INVALID_PARAM);
		BinaryValue v = new BinaryValue();
		int r = native_read(native_handle, v);
		if (r != SUCCESS)
			throw new CapsException(r);
		return v.value;
	}

	public Caps readCaps() throws CapsException {
		if (native_handle == 0)
			throw new CapsException(ERROR_INVALID_PARAM);
		CapsValue v = new CapsValue();
		int r = native_read(native_handle, v);
		if (r != SUCCESS)
			throw new CapsException(r);
		return new Caps(v.value);
	}

	private static class IntegerValue {
		public int value;
	}
	private static class FloatValue {
		public float value;
	}
	private static class LongValue {
		public long value;
	}
	private static class DoubleValue {
		public double value;
	}
	private static class StringValue {
		public String value;
	}
	private static class BinaryValue {
		public byte[] value;
	}
	private static class CapsValue {
		public long value;
	}

	private static native void native_init(Class<?>[] classes);
	private static final Class<?> nativeValueClasses[] = {
		IntegerValue.class,
		FloatValue.class,
		LongValue.class,
		DoubleValue.class,
		StringValue.class,
		BinaryValue.class,
		CapsValue.class
	};
	static {
		System.loadLibrary("flora-cli.jni");
		native_init(nativeValueClasses);
	}

	private native long native_create();
	private native void native_destroy(long h);
	private native int native_write_integer(long h, int v);
	private native int native_write_float(long h, float v);
	private native int native_write_long(long h, long v);
	private native int native_write_double(long h, double v);
	private native int native_write_string(long h, String v);
	private native int native_write_binary(long h, byte[] v, int offset, int length);
	private native int native_write_caps(long h, long v);
	private native int native_read(long h, IntegerValue v);
	private native int native_read(long h, FloatValue v);
	private native int native_read(long h, LongValue v);
	private native int native_read(long h, DoubleValue v);
	private native int native_read(long h, StringValue v);
	private native int native_read(long h, BinaryValue v);
	private native int native_read(long h, CapsValue v);

	public static final int SUCCESS = 0;
	public static final int ERROR_INVALID_PARAM = -1;
	public static final int ERROR_WRONLY = -4;
	public static final int ERROR_RDONLY = -5;
	public static final int ERROR_INCORRECT_TYPE = -6;
	public static final int ERROR_END_OF_OBJECT = -7;

	long native_handle = 0;
}
