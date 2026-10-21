/* SPDX-License-Identifier: GPL-2.0-only */
package org.missioncontrol.stadia.scanprobe;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public final class ScanEvidenceTest {
    private static byte[] record(boolean responseHid) {
        byte[] hid = {3, 2, 0x12, 0x18};
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        if (!responseHid) out.writeBytes(hid);
        out.writeBytes(new byte[]{2, 1, 5, 3, 0x19, (byte)0xc4, 3, 16, 9});
        out.writeBytes("StadiaTest-0000".getBytes(StandardCharsets.US_ASCII)); // Fictional.
        out.writeBytes(new byte[]{2, 0x0a, (byte)0xf8, 3, 0x19, (byte)0xc4, 3});
        if (responseHid) out.writeBytes(hid);
        return out.toByteArray();
    }
    private static final class PrivatePeer {
        @Override public String toString() { throw new AssertionError("Identity must never be formatted"); }
    }
    private static ScanEvidence.Session session(Object peer, byte[] data) {
        ScanEvidence.Session s = new ScanEvidence.Session(0);
        for (int i = 1; i <= 3; ++i) s.accept(peer, true, 0, true, data, i);
        return s;
    }
    public static void main(String[] args) {
        ScanEvidence.LaunchGate gate = new ScanEvidence.LaunchGate(false);
        assert gate.consume() && !gate.consume();
        ScanEvidence.LaunchGate recreated = new ScanEvidence.LaunchGate(gate.consumed());
        assert recreated.consumed() && !recreated.consume(); // Configuration/process saved-state recreation never grants another scan.
        ScanEvidence.LaunchGate beforeStart = new ScanEvidence.LaunchGate(false);
        assert new ScanEvidence.LaunchGate(beforeStart.consumed()).consume(); // Permission-only recreation may still reach its FIRST start.
        byte[] raw = record(true);
        assert raw.length == 35;
        assert ScanEvidence.parse(raw).hidAdOffset == 31;
        assert ScanEvidence.parse(record(false)).hidAdOffset == 0;
        assert ScanEvidence.parse(Arrays.copyOf(raw, 62)).hidAdOffset == 31;
        assert ScanEvidence.parse(new byte[63]) == null;
        assert ScanEvidence.parse(new byte[]{3, 2, 0x12}) == null;
        assert ScanEvidence.parse(new byte[]{2, 2, 0x12}) == null; // Odd UUID16 data.
        assert ScanEvidence.parse(new byte[]{1, 0x19}) == null;
        assert ScanEvidence.parse(new byte[]{0, 1}) == null;
        assert ScanEvidence.parse(null) == null;
        assert ScanEvidence.parse(new byte[0]) == null;
        Object peer = new PrivatePeer();
        ScanEvidence.Session s = session(peer, raw);
        Arrays.fill(raw, (byte) 7); // No input arrays retained.
        s.tick(20_000);
        String report = s.json("stop_requested_api_void");
        assert report.contains("qualified_android_record") && report.contains("\"hid_ad_offset\":31");
        assert !report.contains("StadiaTest") && report.contains("NOT an over-air");
        assert report.contains("\"candidate_peers\":1");
        s.accept(new PrivatePeer(), true, 0, true, record(true), 20_001);
        assert s.json("stop_requested_api_void").equals(report); // Closed session ignores later callbacks.
        s = new ScanEvidence.Session(0);
        s.accept(peer, true, 0, true, record(true), 1);
        s.accept(new PrivatePeer(), true, 0, true, record(true), 2);
        assert !s.running() && s.json("stop_requested_api_void").contains("ambiguous");
        s = new ScanEvidence.Session(0);
        s.accept(peer, true, 0, false, record(true), 1);
        s.accept(peer, false, 0, true, record(true), 2);
        s.accept(peer, true, 2, true, record(true), 3);
        s.tick(20_000);
        assert s.json("stop_requested_api_void").contains("\"candidate_peers\":0");
        assert s.json("stop_requested_api_void").contains("inconclusive");
        s = session(peer, record(true)); s.tick(2);
        assert s.json("stop_requested_api_void").contains("clock_regression");
        s = session(peer, record(true)); s.end("backgrounded");
        assert s.json("stop_requested_api_void").contains("backgrounded");
        assert s.json("stop_exception").contains("stop_exception");
        s = new ScanEvidence.Session(0);
        for (int i = 0; i <= ScanEvidence.MAX_REPORTS; ++i) s.accept(peer, false, 0, true, null, 0);
        assert s.json("stop_requested_api_void").contains("report_limit");
        s = new ScanEvidence.Session(0);
        s.accept(peer, true, 0, true, record(true), 20_000);
        assert s.json("stop_requested_api_void").contains("\"candidate_peers\":0"); // Expired sample cannot qualify.
        s = session(peer, record(true));
        for (int i = 1; i <= 8 && s.running(); ++i) {
            byte[] variant = Arrays.copyOf(record(true), 35 + i);
            s.accept(peer, true, 0, true, variant, 4 + i);
        }
        assert !s.running() && s.json("stop_requested_api_void").contains("layout_limit");
        System.out.println("Android record model: bounded parsing, raw0-independent AD offsets, unique peer, freshness, deadlines, retirement and privacy passed.");
    }
}
