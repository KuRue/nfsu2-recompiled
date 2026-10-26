package com.nfsu2.androidevolved;
public final class ReleaseVersionTest {
    static void check(boolean value) {if(!value)throw new AssertionError();}
    public static void main(String[] args) {
        check(ReleaseVersion.newer("v0.14.11", "0.14.10-phone-dev-physics"));
        check(!ReleaseVersion.newer("v0.14.10", "0.14.10-phone-dev-physics"));
        check(!ReleaseVersion.newer("v0.14.9", "0.14.10-phone-dev-physics"));
        check(ReleaseVersion.newer("v0.10.0", "0.9.99"));
        check(!ReleaseVersion.newer("v0.14.11-beta", "0.14.10"));
        check(!ReleaseVersion.newer("garbage", "0.14.10"));
        check(!ReleaseVersion.newer("v0.14.11", "0.14.10-unrecognized"));
    }
}
