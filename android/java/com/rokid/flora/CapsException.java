package com.rokid.flora;

public class CapsException extends Exception {
	public CapsException(int code) {
		errCode = code;
	}

	public int errCode() {
		return errCode;
	}

	private int errCode;
}
