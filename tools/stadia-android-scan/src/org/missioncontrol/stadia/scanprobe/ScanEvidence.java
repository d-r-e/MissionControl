/* SPDX-License-Identifier: GPL-2.0-only */
package org.missioncontrol.stadia.scanprobe;

import java.util.ArrayList;
import java.util.List;

/** Android-record evidence, NOT an over-air packet decoder. No raw data retained. */
public final class ScanEvidence {
    public static final long WINDOW_MS = 20_000;
    public static final int MAX_REPORTS = 4096;
    private static final byte[] PREFIX = {'S', 't', 'a', 'd', 'i', 'a'};

    /** Persist consumed() across Activity recreation; UI/permission events cannot reset it. */
    public static final class LaunchGate {
        private boolean consumed;
        public LaunchGate(boolean restoredConsumed) { consumed = restoredConsumed; }
        public boolean consumed() { return consumed; }
        public boolean consume() {
            if (consumed) return false;
            consumed = true;
            return true;
        }
    }

    public static final class Layout {
        public final int rawLength, hidAdOffset;
        private final int[] offsets, lengths, types;
        private final boolean candidate;
        private Layout(int size, int hid, List<Integer> off, List<Integer> len,
                       List<Integer> typ, boolean matches) {
            rawLength = size; hidAdOffset = hid; candidate = matches;
            offsets = off.stream().mapToInt(Integer::intValue).toArray();
            lengths = len.stream().mapToInt(Integer::intValue).toArray();
            types = typ.stream().mapToInt(Integer::intValue).toArray();
        }
        private String json() {
            StringBuilder s = new StringBuilder("{\"record_bytes\":").append(rawLength)
                .append(",\"hid_ad_offset\":").append(hidAdOffset)
                .append(",\"hid_ad_at_or_after_legacy_primary_cap\":").append(hidAdOffset >= 31)
                .append(",\"ad\":[");
            for (int i = 0; i < offsets.length; ++i) {
                if (i != 0) s.append(',');
                s.append("{\"offset\":").append(offsets[i]).append(",\"length\":")
                    .append(lengths[i]).append(",\"type\":").append(types[i]).append('}');
            }
            return s.append("]}").toString();
        }
    }

    /** Only AD headers + known HID offset survive; no names/addresses/values. */
    public static Layout parse(byte[] raw) {
        if (raw == null || raw.length == 0 || raw.length > 62) return null;
        List<Integer> offsets = new ArrayList<>(), lengths = new ArrayList<>(), types = new ArrayList<>();
        boolean name = false, appearance = false;
        int hid = -1;
        for (int p = 0; p < raw.length;) {
            int len = raw[p] & 255;
            if (len == 0) {
                for (int q = p; q < raw.length; ++q) if (raw[q] != 0) return null;
                break;
            }
            if (p + 1 + len > raw.length) return null;
            int type = raw[p + 1] & 255, data = p + 2, count = len - 1;
            offsets.add(p); lengths.add(len); types.add(type);
            if (type == 0x09 && count >= PREFIX.length) {
                boolean prefix = true;
                for (int q = 0; q < PREFIX.length; ++q) prefix &= raw[data + q] == PREFIX[q];
                name |= prefix;
            }
            if (type == 0x19) {
                if (count != 2) return null;
                appearance |= (raw[data] & 255) == 0xc4 && (raw[data + 1] & 255) == 3;
            }
            if (type == 2 || type == 3) {
                if ((count & 1) != 0) return null;
                for (int q = 0; q < count; q += 2)
                    if ((raw[data + q] & 255) == 0x12 && (raw[data + q + 1] & 255) == 0x18 && hid < 0) hid = p;
            }
            p += len + 1;
        }
        return new Layout(raw.length, hid, offsets, lengths, types, name && appearance && hid >= 0);
    }

    public static final class Session {
        private final long start;
        private long last;
        private Object peer; // Framework identity RAM-only; never formatted, hashed or serialized.
        private int reports, stale, malformed, targetReports, peers;
        private final List<Layout> layouts = new ArrayList<>();
        private String outcome = "running";
        public Session(long now) {
            if (now < 0 || now > Long.MAX_VALUE - WINDOW_MS) throw new IllegalArgumentException("clock range");
            start = last = now;
        }
        public boolean running() { return outcome.equals("running"); }
        public void tick(long now) {
            if (!running()) return;
            if (now < last) { end("clock_regression"); return; }
            last = now;
            if (now - start >= WINDOW_MS) end(peers == 1 && targetReports >= 3 ? "qualified_android_record" : "inconclusive");
        }
        public void accept(Object identity, boolean legacy, int dataStatus, boolean fresh,
                           byte[] raw, long now) {
            tick(now);
            if (!running()) return;
            if (++reports > MAX_REPORTS) { end("report_limit"); return; }
            if (!fresh) { ++stale; return; }
            if (!legacy || dataStatus != 0 || identity == null) return;
            Layout layout = parse(raw);
            if (layout == null) { ++malformed; return; }
            if (!layout.candidate) return;
            if (peer == null) { peer = identity; peers = 1; }
            else if (!peer.equals(identity)) { peers = 2; end("ambiguous"); return; }
            ++targetReports;
            String shape = layout.json(); // Contains metadata only.
            for (Layout old : layouts) if (old.json().equals(shape)) return;
            if (layouts.size() == 8) { end("layout_limit"); return; }
            layouts.add(layout);
        }
        public void end(String reason) {
            if (!running()) return;
            switch (reason) {
                case "qualified_android_record": case "inconclusive": case "clock_regression":
                case "report_limit": case "ambiguous": case "layout_limit": case "backgrounded":
                case "operator_stop": case "scan_failed": case "start_exception": break;
                default: throw new IllegalArgumentException("unknown outcome");
            }
            outcome = reason; peer = null;
        }
        public String json(String cleanup) {
            if (running()) throw new IllegalStateException("scan still running");
            if (!cleanup.equals("stop_requested_api_void") && !cleanup.equals("stop_exception"))
                throw new IllegalArgumentException("unknown cleanup");
            StringBuilder s = new StringBuilder("{\"schema\":1,\"outcome\":\"").append(outcome)
                .append("\",\"cleanup\":\"").append(cleanup)
                .append("\",\"semantics\":\"Android normalized or reassembled record; NOT an over-air PDU capture\"")
                .append(",\"callbacks\":").append(reports).append(",\"stale\":").append(stale)
                .append(",\"malformed\":").append(malformed).append(",\"candidate_peers\":").append(peers)
                .append(",\"target_reports\":").append(targetReports).append(",\"layouts\":[");
            for (int i = 0; i < layouts.size(); ++i) { if (i != 0) s.append(','); s.append(layouts.get(i).json()); }
            return s.append("]}\n").toString();
        }
    }
    private ScanEvidence() {}
}
