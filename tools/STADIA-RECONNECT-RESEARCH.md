# Stadia reconnect: current evidence and bounded research

Research only. No console/config/radio changes in this investigation. Installed diagnostic
remains `5ed1417eef4917b85e857374c24dd51e52e39168e0e8d6d0137731a725e62e73`.
The pre-PR matrix is `STADIA-PRE-PR-CHECKLIST.md`; task33 owns reconnect research.

## Latest read-only console snapshot

Bench cache: `reconnect-research-20261022T193418Z/`.
`ble.log`:9243bytes, SHA-256
`d75e8922899b50ed46bbbedc90b1171ef9ba60370728730c3619632027a8a0bd`.
Module, INI and active boot flag were retrieved read-only; module/INI hashes match deployment.
No pairing, reboot, launch, output or radio command was sent by the investigator.

- Successful fresh link74.532s, presentation81.592s.
- Disconnect133.548s, reason0x0008, followed by awake-only timing dump. No PrepareForSleep
  appears in this snapshot. Reason alone does not prove a particular physical cause.
- Configured recovery radio cycle134.743s; GATT client recovered140.762s.
- No second presentation appears in this particular snapshot. Operator separately reports
  full pairing from pairing mode works; do not invent a second traced same-boot success.

### What the timing sample actually says

| Measurement | Samples | Logged integer average | Histogram bound |
|---|---:|---:|---|
| Slot-read IPC |56|57us|all<=500us|
| Delivery lock |57|0us|all<=100us|
| Translation+injection |57|55us|all<=500us|
| Listener input work |53|121us|all<=500us|
| All-HID FIFO write |72|1us|all<=100us|
| Approximate pre-write FIFO occupancy |72|0bytes|all exactly0bytes|

FIFO write failures:0. Delivery/translation measurements include scheduled neutral reports;
FIFO measurements include other HID producers, not just Stadia. Integer-average0 is not proof
of zero execution time. Notification-return gaps (52samples) include idle time; their average
is not a Bluetooth polling-rate measurement.

This sample contains no evidence of injection congestion or FIFO backlog. It does not measure
radio arrival, pre-wake scheduling latency, consumer/display time, or physical press-to-visible
latency, and does not disprove previously reported intermittent lag. No priority tweak is
justified from this sample alone. Reboot-initiation menu slowdown remains a separate observation.

## Local implementation: five separate reconnect questions

All observations below are from `mc_mitm/source/ble/ble_hid_host.cpp` and `ble_events.cpp`.

1. **Address persistence.** `SaveRememberedDevice()` retains the address in RAM, but writes
   `ble_last_device.bin` only with `auto_connect=true`; `LoadRememberedDevice()` is gated the
   same way. Default is false and this bench config has not enabled it. No keys are stored in
   that file. Address remembering is not bilateral bonding.
2. **Connection initiation.** Outside Change Grip/Order, host scanning is gated on
   `BackgroundScanWanted()` and auto-connect. With this disabled, the host does not initiate
   those background scans. A native connection could still arrive and be adopted for the
   previously attempted address; absence of a host scan is not proof no stack path exists.
3. **Destructive fresh-connect policy.** `Connect()` always calls `btdrvRemoveBond()` before
   the direct connect request. That is an intentional inherited stale-bond workaround. An
   unmodified test of this path cannot demonstrate retained-bond reencryption: it removes the
   local record first. Do not simply remove the workaround from the working build.
4. **Resumption versus fresh security.** An adopted/native connection skips `Connect()`, but
   still enters `Pair()` and calls `btdrvCreateBond()`, waiting on a heuristic lifecycle event.
   Whether that native command can reuse an existing bond is NOT established; its name alone
   cannot prove a fresh SMP exchange. No positively verified resumption state is exposed here.
5. **Recovery reset.** Current `radio_cycle=true` invokes the existing configured Bluetooth
   crash/recovery path after an awake disconnect. That must be accounted for when researching
   retained native state. The log does not prove whether recovery erased any security database.

These are separate from whether Stadia itself retains a legacy bond. Switching on address
persistence alone cannot establish a correct normal-power-on reconnect implementation.

## Public-source cross-check

- [Google: Auto-connecting a paired Stadia Controller](https://support.google.com/stadia/answer/13067284?hl=en)
  explicitly describes blinking-white automatic connection after ordinary power-on. Pairing
  with a different host uses Stadia+Y for2seconds until orange. This establishes expected
  controller behavior on supported hosts, not Switch security compatibility.
- [Bluepad32 pinned LE setup](https://github.com/ricardoquesada/bluepad32/blob/80d6314e46f871719fbb4b721a2cc02227c4f369/src/components/bluepad32/bt/uni_bt_le.c#L980-L1015)
  has active `sm_set_authentication_requirements(SM_AUTHREQ_BONDING)`, with SC-only disabled.
  Comments record Stadia working with several configurations. These are not documented
  controller/host power-cycle retention tests and do not establish the actual negotiated mode.
- The same reference [handles reencryption separately](https://github.com/ricardoquesada/bluepad32/blob/80d6314e46f871719fbb4b721a2cc02227c4f369/src/components/bluepad32/bt/uni_bt_le.c#L671-L755),
  including missing-key handling. Its [POSIX example](https://github.com/ricardoquesada/bluepad32/blob/80d6314e46f871719fbb4b721a2cc02227c4f369/examples/posix/src/main.c#L107-L208)
  configures persistent storage generically. Neither is Stadia-specific retention evidence.
- BTstack's `sm_request_pairing()` can request reencryption when a stored suitable key exists;
  an API's pairing-oriented name is not sufficient to classify its actual operation. Do not
  automatically transfer BTstack semantics to btdrv/BSA.

**Conclusion:** current blanket documentation that Stadia retains bonds ONLY with Secure
Connections remains unverified. Neither legacy Just Works success nor generic reference-host
persistence proves the missing Switch behavior. Do not label this an unavoidable hardware limit.

## Next engineering questions; no new operator test yet

1. Trace actual native bond-request initialization and whether public/non-secret policy fields
   establish bonding/SC requirements. Existing saved wrapper evidence at0x800316d0 is insufficient
   to name all parameters or infer capability. A bounded parent-session follow-up is now saved
   below; no runtime key/database reads, new exports or raw security captures.
2. Decide which existing connection/scanning path can safely observe ordinary-power-on
   advertisements without preemptively deleting the bond. Design one discriminating test, not
   another repeated fresh pairing that answers the wrong question.
3. Only after evidence supports a change, propose the smallest separate fresh-pair/resume
   workflow, retaining setup bounds, ownership and first-failure stop conditions. Build/test and
   explicit deployment approval are separate from this research.

## Bounded native follow-up: corrected a misleading function lead

The child analysis could not see the parent's open REA target and stopped with
`target_unavailable`. A parent-only follow-up succeeded; no provider installation,
configuration change, CLI fallback or console access was needed. `current_document` is not
supported by Ghidra; supported procedure queries were used instead. Session now closed.

Approved btdrv image identity:
`4819eb58c248543cf2a1d991a8245997ae60e1726acb8ef97ffdb3f44c4e218a`,
arm64/raw load0x80000000. Full parent Evidence bundle:
`reconnect-native-research/parent-session-evidence.json` (11records).
Eight new function bodies were inspected, then this pass stopped at its declared bound.

- The old file named `bond-0x800316d0.json` must NOT be treated as a verified CreateBond
  implementation. It validates a length1..16, copies address6bytes plus a16byte field and
  length at0x16, and submits a32byte message via0x800483b0/op0x12f.
  Initializer0x80045e50 merely clears32bytes
  (Evidence `ev_805913ba5df439f27ece2d673ae56adf79adcde1860cc45618a05ffc1421cb4f`);
  submitter checks byte0x16<=16
  (`ev_9451cc242af28fe3225a7c90bb7cda879b306f26bf7e475c821cf2debb2b1c4c`).
  This strongly matches the public BSA PIN-code-reply structure, not a bonding-policy field.
- Public [bsa_sec_api.h](https://raw.githubusercontent.com/khadas/buildroot_vendor_broadcom_brcm-bsa/buildroot/3rdparty/embedded/bsa_examples/linux/libbsa/include/bsa_sec_api.h)
  defines `tBSA_SEC_PIN_CODE_REPLY` with address, PIN_CODE, pin_len, ble_passkey and flags.
  It separately defines `tBSA_SEC_BOND` with only an address. Public structural correspondence
  is inference, not proof of this firmware's exact command binding/version.
- A nearby candidate0x80031550 initializes/copies a6byte address and calls0x80048420
  (`ev_50726be9eb89675174e820374fb361da02982ebbd1450dd453ee570a71926d2d`).
  Submitter0x80048420 sends op0x12d with exactly6bytes
  (`ev_486925e83f0c484a763fd57a53012f651bf850432f84c65cdb7147d02f936414`).
  This matches the public BSA bond-request shape and surrounding inquiry/security handlers,
  but binding it positively to HIPC CreateBond command10 remains unproved.
- The SDK does establish CreateBond command10 and RespondToPinRequest command13. Native
  resolved callers/xrefs for the old wrapper were empty; indirect/relocated dispatch remains
  unknown. Do not elevate neighboring-function order into definitive API identification.

**Result:** no evidence here establishes SC-only behavior, bond-retention impossibility, or a
safe per-request bonding/authentication knob. The zeroed old structure is NOT evidence bonding
was disabled: its presumed API identity was misleading. Next native question is exact dispatch
binding and module-wide security policy, not guessing bits in a PIN-code reply or changing the
working transport argument.

## Operator role

No new action is required now; the requested FTP download is complete. Operator supplies the
physical evidence source code cannot: exact power-on versus pairing gesture and LED behavior,
console awake versus asleep, input feel versus regular-controller baseline, and occasional
one-scenario-at-a-time tests. Stop on the first failure and enable FTP for evidence. The
investigator handles implementation, offline/native research, tests, builds and backups.
No keys, firmware dumps, permission changes or repeated blind pairing are requested.
