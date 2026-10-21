# Historical BLE / Stadia Investigation Notes

These are archived observations and superseded plans. Use [HANDOFF.md](../HANDOFF.md) for current state and instructions. Historical deployment/readiness statements do not establish current hardware state.

> **Latest checkpoint — Android HID descriptor reads BLOCKED by BLUETOOTH_PRIVILEGED; user asks nativeMac instead.**
> User17.05.34Desktopimageonlycatalog(no2908values). AskedONEfirst2908read,
> reportednoeffect; directedShowlog. Userreports exceptionrequires
> BLUETOOTH_PRIVILEGED. Verifiedandroid16-release AOSP GattService readDescriptor:
> restrictedReport2a4dhandleanddescriptors=>permissionCheck=>privilegedenforcement
> BEFORE nativeGattReadDescriptor; bondnotexemption; oldtargetSDKsuppresses
> exceptionbutstillNOread. No oldAPK/root/grant/rebond retries. Todo49PENDINGblocked.
> Updatedsanitizedfixture+archive reference_read_attempt; no valuesobtained.
> AskeddisconnectnRF/AndroidBToff,bondintact. PhoneoffNOTyetconfirmed.
> User 'can't you use macOS bluetooth?' AnsweryesnativeMacIOHIDalready343valid,
> rawGATTseparatelimitation. ReadALLexistinggatt-capability.swift: uses
> discoverServices(nil), equalityCBUUID1812, completesservicescallback thenchecks
> HID; no narrowservicefilterbugidentified. EarlierliveCoreBTsaw2services/no1812,
> cleanedownappref; no needunchangedretry/notproofeverypossibleMacrouteunavailable.
> PublicAppleforumsearchconsistentwithOS-handledHIDfiltering,butfetchesincomplete:
> doNOTclaimfullyverifiedcrossplatformDTSquote. No newMacBluetoothactivity,
> no privateAPIs/PacketLogger/fullHCI/keys, no installs or permissionchanges.
> Android16sourceconfirmed https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/heads/android16-release/android/app/src/com/android/bluetooth/gatt/GattService.java
> ForfutureSwitchStadia tests bothMacBToff+AndroidBToffrequired nowphonebonded.
>
> Previous checkpoint — operator accepted Android pairing; CONNECTED/BONDED thenDISCONNECTED/BONDED in follow-up images.
> Userexplicitlyreports 'now connected,i had to accept the pairing'. Reviewed
> newDesktopimages17.01.10.jpg(CONNECTED/BONDED) and17.01.14.jpg(DISCONNECTED/
> BONDED). Userliveconnectedclaimmaypostdatesecondimage; don'tassumecurrentlink.
> Same1812catalog/two2a4drows.Noattributevalues/handles/notificationscaptured.
> Updatedsanitizedtests/fixtures/stadia_android_gatt_summary.txt andmanifest
> android_stadia_gatt_metadata.bond_followup. Rawimagesnotcopied/keysnotread.
> BondstatechangedbyOPERATOR, nottool-issuedpair. No unpair/reset requested.
> DoNOTinferallattrsrequirebonding or explainSwitchraw0x1a viaAndroidsecurity.
> NEWPRECONDITION forfutureSwitchStadia trials: ANDROIDBTmustalsoOFF (inaddition
> Macoff), becausebondedphonecanautoreconnect; appDisconnectisnotunpair.
> Nexttodo49PENDING: manualmetadataonly, connectedstate read2908ReportReference
> underNOTIFY/READreportandunderREAD/WRITEreport, oneeach; ReportID/typeonly.
> DoNOTreadReport2a4dvalues/enableNotify/writeCCCD/protocol/output. ThenDisconnect.
> NeedlivevaluesbeforeclassifyingsecondReportOutput/Feature; notpredictions.
> No newautomatedBLEtest/appdeployment/radiorequestlaunchedduringimagereview.
>
> Previous checkpoint — Android Stadia1812 catalog VISIBLE; BONDING seen thenDISCONNECTED/NOTBONDED. No raw GATT input yet.
> UserAndroid16;authorizedUSBdebug. ExactlyoneADBdevice, queriedONLYrelease16,
> SDK36,SDKfull36.0. No identifiersretained/appinstalled/capture/radiorequest.
> PassiveAPI36.1unavailable; todo47stillPENDING/blockedbycapability.
> Askedstopadvertising/Macoff/Stadiaorange,nRFscanidentify; userdone.
> ThenmanualONEConnect, cancelpairpromptifany, no read/write/notify/bond, inspect
> 1812and2a4dcount thenDisconnect. UserprovidedtwoDesktopimages reviewed:
> 2026-10-20 16.57.27.jpg earlierCONNECTED/BONDING...;16.57.34.jpg later
> DISCONNECTED/NOTBONDED. HID1812PRIMARYvisible. 2visible2a4dReportchars:
> (NOTIFY,READ) and(READ,WRITE), BOTHshow2902and2908descriptors. Also2a4eRW,
> 2a4bREAD,2a4aREAD,2a4cWRITE. No values/handles/instancescaptured.
> DO NOT labelsecondReportOutputvsFeatureuntilreferencevalue; UUIDnotunique.
> OneNotifycandidatehelpsprofileunderstandingbutdoesNOTsolvecallbackowner/
> instance/generationcorrelation. No ReportMap/Ref/notification readsvalidated.
> Androidservicevisibilityreal; MacCoreBluetoothmissing1812wasnotproofabsent.
> BONDINGisimportant: automaticsecurityobserved, initiator/reasonunknown,
> no completedbondshown. NEXT ASKUSER: didpairpromptappearanddidyoucancel,
> or connectionendedautomatically? No newconnect/read/subscribeuntilclarified.
> Sanitizedtests/fixtures/stadia_android_gatt_summary.txt+P3manifest updated;
> rawscreenshotsNOTcopied(noaddresses/fullname/keys/payloadretainedinrepo/archive).
> Todo48COMPLETE(metadataobservation,nottransport). No radiooperationfromtools
> duringimageinspection. No backgroundtest/helperrunning.
>
> Previous checkpoint — SAME-BOOT response-onlyHID control NEGATIVE, pairedprimary+3 vsresponse+0.
> Userconfirmedadvertising/netloaderafterinstructedmoveONLY1812toScanResponse,
> keepdiagnostic128primary. MacoffPASS, existing0.6.25--discovery-requests,
> containerf0c77caefbbe... freshA30sec575polls. Cumulativeads3->3,HID3->3,scan
> events3->3: fresh0. NativeBTM1start1stop1params,HID0;managedStart/Stop0.
> Filters2add2removeactive0failed0cleanup1trusted1;no aliases/connect/pair/GATT.
> sizeConvention2->2HISTORICALnotnewcontrolattribution;target_seen0expectedAndroid.
> Archiveandroid-response-hid-control.log+manifest. Todo46COMPLETE.
> ToldstopAndroidadvertising/PLUS. PairedplacementdependenceprovenUNDERTESTED
> path, NOTuniversalBLElimitation/NOTStadiaactualPDUboundary(active merged35B).
> Offlinepubliccontractcheck switchbrew.org/wiki/Bluetooth_Driver_services:
> BleAdvertiseFilter supportsONLY2-3UUID16,4-5UUID32,6-7UUID128,255Manufacturer.
> NAME/Flags/Appearance/targetADDRESSNOTdocumentedfilterconditions. NOguessed
> local-namefilterimplementation! Cmd55noargs;98interval/window2u16only; no
> documentedactive/passivescanningselector/filterdeliverymode. No nativewrites.
> Newtodo47PENDING: choosevalidStadiaprimary-v-responseevidence. Candidate newer
> AndroidAPI36.1 SCAN_TYPE_PASSIVE officialdocs says'responsesignored'; SDKBuilder
> setScanType defaultsACTIVE,passivewithoutwaitingforresponses. PhoneOSversion
> UNKNOWN. NeedaskAndroidversionbeforepromisinghelper/apporexistingnRFtoggle.
> No APK/newNRO/modulebuilt/installed; no furtherradioactivitylaunched.
> Sources: https://developer.android.com/reference/android/bluetooth/le/ScanSettings#SCAN_TYPE_PASSIVE
> and /ScanSettings.Builder#setScanType(int). API36.1minorimportant(not36.0).
> EarliercommentsaboutpossibleprimarynamefiltermustNOTbeimplemented:unsupported.
>
> Previous checkpoint — Android PRIMARY-HID positive control SUCCEEDED on current boot/P3, no connection.
> Userconfirmed1812+diagnostic128ending5832bothAdvertisingData,ScanResponseempty.
> MacoffpreflightPASS; launchedexisting0.6.25--discovery-requests atfreshA.
> Container30642195b402... probeawaitsPLUS. ~30sec575polls: 3driverads+3HIDads,
> 3successfulScanResult events0errors; knowncontrolsize_convention0->2 (fresh
> recognition). NativeBTMrequestdelta1start1stop1params,HID0. Managedstart/stop0;
> filters1add1removeactive0failed0cleanup1trusted1. No aliases/connect/pair/GATT.
> target_seen0EXPECTEDsinceconfiguredsavedStadia target != syntheticAndroid.
> Archiveandroid-primary-hid-control.log+manifestandroid_primary_controlexactfacts.
> Todo45COMPLETE. ThisprovescurrentreceivepathworksforprimaryHID/control, NOT
> StadiaactualPDUplacement/nativerequestreturncodes. No rawaddresses/payloadslogged.
> Nexttodo46PENDING: pairedsingle-variabletest moveONLY1812toScanResponse;
> diagnostic128remainsprimary. Samelegacy/connectable,namesomitted,sameP3/NRO/
> native-onlyHIDfilter.MacoffStadiaoff. Asknewadvertiser+netloaderreadiness,
> thenONE30sec--discovery-requests freshA,no connect. Comparefreshdriver/HID
> counterdeltas; priorconvention2ishistorical, notfreshcontrolrecognition.
> Earlierfirmwarecontrolsalreadyshowedplacementdependence; thiscurrentbootP3
> controlledpaircheckswithnewnativeattempttelemetry, notaproposedfix.
>
> Previous checkpoint — UI clarified/exit-only recovery; user explicitly RESUMES RADIO TESTS, Android primary-HID control next.
> Afterrebootuserenablednetloader; ran0.6.25--event-progressONLY (no btdev/btmu
> initialization/newradiooperations). FreshA12samplescomplete: P3IDcorrect,
> cmd13proto1enabled1BLEinit1; filters0/0active0failed0; nativeStart/Stop0params2;
> all12lanesidle/balanced. Archivedrecovery-readonly.log. Userthenreportedslow.
> IMPORTANTCORRECTION: Userclarifiedmainmenu/HOME/everythingNORMAL; ONLYsidebar
> andpowermenuslow. AskedPLUSexit WITHOUTreboot; userreported 'now,no slowliness'.
> Thisisprobe-lifetime-associatedmenu latency, notestablishedpersistentBTfault.
> Initiallydeferredradio, butuserexplicitlysaid 'no; continue the radio tests,
> the slowliness can come later'. Followlatestdirection: radioallowed,menuissue
> deferred(todo44PENDING). No speculativefocus/renderchangesmade.
> Nexttodo45PENDING: Android/nRFConnectknownpositivecontrol SAME primary payload
> UUID16 1812 +UUID128 9f6dca74-69b7-4a35-9f01-9ab841ab5832, legacy/connectable,
> scanresponseempty, nodevicename/GATTserver. StadiaOFF, MacBTOFF, netloaderON.
> Existing0.6.25--discovery-requests supportsone native-HID scoped filter; no
> moduleupdate/newNROneeded. ForANDROIDcontrol savedStadiatarget_seen0EXPECTED;
> lookforFRESHdriver/HIDadsandknowncontrolsizeconvention, notarbitrarypeers.
> No Androidrunlaunchedyet; requestadvertiserreadiness,thenfreshA. No connect.
> Archiveui_clarification supersedesold 'system slow/rebootneeded/radiofrozen'
> interpretations below. Task44unresolvedbutexplicitlynotablockperoperator.
>
> Previous checkpoint — HOME SLOW after confirmation scan; user rebooted and reports normal again. RADIO TESTS PAUSED.
> Afterrepeat userexplicitlyreported 'home is slow now'. Toldstoptesting/poweroff
> Stadia/PLUSexitifresponsive, rebootviaHekateifslowpersists. Usernowreports
> 'all good after reboot'. Thisisoperator-confirmedresponsivenessrecovery, NOT
> proofcauseidentified/fixed. No radiooperationornewdeploymentafterreport.
> P3archivemanifestrepeat_request+recoveryupdated. Filtercleanupwasreal,butdid
> NOTestablishsystemrecovery. PreviousidleposteventsnapshotsoccurredINSIDE
> DiscoverSavedStadia BEFORE RunDiscoveryRequests callsbtdevExit; thuslate-event/
> teardownstallisplausiblebutUNVERIFIED. Do notblamecounters/decoder/BTMwithout
> evidence. Freshbootloadedcodestatusnotrequeriedafterrecovery.
> Newtodo44PENDINGofflinepost-scanstop/clientteardown/shared-event-waitanalysis.
> No furtherAndroidcontrol/Stadiascan/connectuntilthissafetyblockaddressed.
> PairingcanremainOFF. No backgroundjobs/reproductionrunning.
>
> Previous checkpoint — user-confirmed orange pairing; explicitly requested repeat also0ads/clean cleanup.
> UseransweredStadiaWASorangeinfirst30secscan, thenexplicitlyaskedresendandwould
> reenablepairing. Honoredoneexplicitconfirmationrepeat(notfilterfix/newhypothesis).
> Previousprobeexited; launched--discovery-requests container367e08a8cd0e... with
> MacBToffpreflightPASS. ToldsetorangeAFTERscreenappears,thenfreshJoyConA.
> Repeatcompleted30sec575polls,target0,trackedads0; BTMnativeattemptdelta1start/
> 1stop/1params,HID0. ManagedStart/StopIPC0;filtertotalsnow2add2removeactive0
> failed0,cleanup1trusted1. No connect/pair/GATT/cancel/output. Logdiscovery-requests-
> repeat-1.log+manifestrepeat_request inP3archive. Thisisnotunchangedunapproved
> retry: operatorexplicitlyrequestedconfirmation. Do notdoathirdidenticalrun.
> USER HOME/sidebarresponsivenessstillnotexplicitlyconfirmed. Nextmeaningfultest
> candidate: same-bootknownAndroid/nRFprimaryHIDadvertisementpositivecontrol vs
> Stadia, toseparatefilter/radioreceptionfromcontrollerADplacement. ExistingP3
> discovery-onlymodecanobserveHIDdrivercountdeltaswithoutconnecting; ifusedfor
> control, saytarget_seen0expected(savedStadiaaddress), requirefreshknowncontrol
> evidence/UUIDtag ratherthanattributearbitraryHIDpeers. Controlserveravailability
> andUIstate needoperatorinput; no controlscanstarted. Stadiapairingcanstopnow.
>
> Previous checkpoint — P3 loaded/BLE-ready VERIFIED; live discovery-request probe COMPLETED CLEANLY,0 filtered ads.
> AfteruserHekateboot/netloader,0.6.25--event-progressfreshAverifiedactualP3codeID
> 473264005b02fcd9412f65ae607c8794ec80b1c6000000000000000000000000,
> cmd13proto1enabled1BLEinitialized1,cache2,filters0/0active0failed0,connectionIdle.
> StartupattemptcountsBTMstart0stop0parameters2,HID0.12lanesidle/balanced.
> Todo43COMPLETE. Readinesslogpost-reboot-readiness.log inP3archive.
> UserthenconfirmednetloaderON/pairingOFF/MacBTOFF. Macoffpreflightpassed.
> TolduserAFTERprobeappears:enableStadiaorangeflashingthenfreshJoyConA.
> RanexactONE0.6.25--discovery-requests,containerf133602e14c5... (stdioawaitsPLUS).
> ~30sec575polls, managedStart/StopIPC0. NativeREQUESTdeltaBTMstart1stop1params1,
> HIDall0. ThisrulesoutNO nativeStartREQUESTduringtest; NOTnativeIPCresult/radio
> stateproof. Filter1add/1remove active0failed0cleanup1trusted1; trackedads0,
> target0; allposteventlanesidle/balanced. No connect/pair/GATT/cancel/output.
> Archive.../scan-request-diagnostic-20261020T122842Z/discovery-requests.log and
> manifestdiscovery_request_run. Todo41COMPLETEwithnegativebutvaliddiagnostic.
> NEXT askPHYSICALconfirmation:WasStadiaactuallyorangeflashingduring30secs?
> IsHOME/sidebarresponsiveafterrun? UserlastsaidpairingOFFbeforeupload; A gate
> happenedbutorangeisNOTindependentlyconfirmed. Do notassumetargetadvertised.
> No immediateunchangedretry. Canstopcontrollerpairing; scanalreadycleanlystopped.
> Older pendingreboot/incompleteprobe notes below are superseded.
>
> Previous checkpoint — P3 scan-request diagnostic DEPLOYED/SD hash verified; await Hekate emuMMC reboot+netloader.
> UserFTP5000ready/pairingOFF. Completedprotectedmodule-onlytransaction:
> stagedcandidateverified, renamedoldexefs->uniqueSDbackup, renamedstage->exefs;
> newreadback183992 SHA460a46c4ccb7b163b55e687a542002131aaf5487baa30b5fd975c6df79ba3e65.
> PriorP2alsoarchivedlocally installed-before-exefs.nsp; SDbackuppath inmanifest.
> ALL99knownfilehashesprechecked; postonlyactiveexefschanged.93normalIPSexact,
> INI/template/mitm/toolbox/bootflag+bootconfigsunchanged, nolooseexefsoverride.
> FTPNLST(path)returnedempty; firstpreflightABORTEDbeforeANYwrite. Investigated
> viaCWD+NLST; additional89IPS._sidecars(sameknownbasenames) present. Protected
> thoseplus5module/flagsmetadata=94sidecars bybefore/afterhash; none removed.
> ParentdirectoryonlynewentryisSDoldmodulebackup; stagedpathgonebyrename.
> Archive ~/.cache/missioncontrol/stadia-backups/scan-request-diagnostic-20261020T122842Z/
> manifestdeploymentphaseVERIFIED,reboot_pendingtrue, Switch_deployedtrue means
> SDONLY; activeprocessstillP2untilreboot, NOT loaded-codeverified/no newscan.
> Todo43IN_PROGRESSremainingloaded-codeverification. AskoperatorexitFTP,boot
> emuMMCviaHekate,enablehbmenunetloader; pairingOFF andreportUIresponsiveness.
> Nextnetload0.6.25 --event-progress(readonly). MustcompareloadedNSObuildID
> 473264005b02fcd9412f65ae607c8794ec80b1c6000000000000000000000000
> andMCcmd13protocol1 enabled1 ble_initialized1; NROclassifiermaylabelnewID
> unknown(itknowsolderarchives), compareactualhexagainstmanifestnotlabel/builddate.
> Afterloaded/readinessproofthenprepareONE --discovery-requests,MacBToff
> preflightandoperatorStadiapairingorange/A gate. NEVERconnectevenpositive.
> No BTscan/connect/settingchangehasoccurredduringdeployment. FTPsessionclosed.
> Older not-deployed/awaitFTP notes below are historical/superseded.
>
> Previous checkpoint — live readiness clean; NEW passive scan-request diagnostic built/tested, AWAIT FTP (not deployed).
> User asked validate sysmodules thenlaunchdiscovery andtellFTP/netload/pairing.
> CorrectSwitchdestinationoperatorprovided .146:28280 onprevioussubnet, cached
> privately /tmp/missioncontrol-readiness-switch-ip (noBluetoothaddress). First
> wrongsavedIPuploadfailed beforeprobe; correcteduploadsactuallyran0.6.24.
> Readonly --event-progress archive~/.cache/missioncontrol/stadia-host/switch-readiness-20261020T121304Z/
> verifiedP2build03ff...cache2scan5, filters1add/1remove active0failed0,
> connectionIdle/unarmed/noactions,12snapshotsidle/balanced (notUIproof).
> Then --service-latency archive.../switch-service-readiness-20261020T121627Z/
> BTMstate6=Initialized, MCqueries489/525us, BTMopen372us/GetState287us. BLE36->40
> betweenprobesbackground/nativeactivitynotourradiochanges. UIresponsivenessnot
> explicitlyconfirmed. ExistingNROiswaitingPLUS; no scan/connecthasrun.
> Meaningfulnextquestion: doesBTMrequestnativeStart/Stop/Parameters vsjustfilter?
> New ble_scan_request_progress.hpp lockfree counters+MCcmd13strictPOD40bytes,
> initializedflag. Btdrvhooks55/56/98 ALWAYSRAWFORWARDoriginal viaShouldForward;
> no new nativecalls/parameterwrites/locks/waits/resets. CountsATTEMPTSnotIPCsuccess
> orhardwareactive/targetownership. Existingevent/cacheABIsunchanged.
> NewNRO0.6.25 --discovery-requests: readinessgates; one30scompleteUUID16filter,
> before/afterrequestcounters; NEVERCONNECTevenpositive. SamefilterNOTa fix;
> newobservationsjustifydiagnosticratherthanunchangedblindscan. Otherpausedmodes
> remainpaused. MacBT-offpreflight+operatorpairingreadyneededONLYafterdeployverify.
> Mac+DockerfullsuitesPASS; reviewerforwarding/cleanupnoblockers; missingMakefile
> include/headerdependenciesfixed,independentmake-n-WchecksPASS. Firstnative
> buildfailedSFPODdefaultinitializers;fixedtrivialPOD, correctednativebuildEXIT0.
> Archive~/.cache/missioncontrol/stadia-backups/scan-request-diagnostic-20261020T122842Z/
> NSP183992 SHA460a46c4ccb7b163b55e687a542002131aaf5487baa30b5fd975c6df79ba3e65
> NRO356408 SHAb8761e2187a6cfbaa45dcec88a0d7e87513bdc68ffeac34c421c6f3320a013c8
> ExpectedP3NSObuildID473264005b02fcd9412f65ae607c8794ec80b1c6000000000000000000000000.
> Todo42COMPLETE(code/offlineverification),43PENDINGprotectedmodule-onlyFTPdeploy,
> 41PENDINGliveprobe blockedBy43. NEXT telloperatorPLUSexit+openFTP+portshown;
> keepStadiapairingOFF. No needinstallNROonSD(netloadit). Backupexefs/hash93IPS+INI+
> bootmetadata, replaceONLYMCexefs,readbackhash,verifyprotectedunchanged. Thenuser
> bootemuMMCviaHekate,netloadREADONLYproveactualP3codeID+cmd13+BLEinitbeforepairing.
> NO P3deployment/radioactionyet. No build/reviewerjobsrunning.
>
> Previous checkpoint — native build VERIFIED SUCCESS; runtime/live BLE integration HARD-BLOCKED on transport evidence.
> Corrected container mc-stadia-controller-build-complete-sdk-20261020 EXIT0,
> MISSIONCONTROL_BUILD_EXIT=0, linked NSO+NSP. Parent independently verified logs
> and SHA256. NSP183203 ae5975be1ed866601320c945717bab7c1c47e05cc8f579462e71625d4b2d4209;
> NSO182143 f8a738045029b80f7227c759b66eb3a20d40f7fb873455b42c82a92f2ed4a706.
> Archive ~/.cache/missioncontrol/stadia-host/switch-gatt-controller-build-20261020T115257Z/
> contains artifacts/build.log/manifest. Controller task39COMPLETE with Mac/Docker
> regression passes + follow-up review no issues; no SDK-controller live test.
> No deployment: new methods not connected to native BLE callbacks/virtual attach.
> Task38remainsIN_PROGRESS, blockedBy40(authoritative transport evidence/new
> discriminating scoped experiment). LocalSDK trace/publicSwitchbrew/researcher/
> directlibnxPR644/658/focusedGoogle upstreamsearch did not establish instance/
> owner mapping or a supported fresh discovery fix. Do not repeat same scan or
> code exports, tag next UUID callback, inject keys/metadata, or guess scan flags.
> User authorized autonomous loop without routine approval; hard evidence/safety
> blockers still apply. No approval waiting, no active background jobs remain.
> The successful build is a useful checkpoint, NOT completed Stadia BLE support.
> Older build-running instructions below are historical/superseded.
>
> Previous checkpoint — Stadia controller GATT delivery seam added/review-fixed; native build RUNNING.
> User authorized continuing in a loop without routine approvals; keep safety
> limits and do not claim activity between turns unless actually launched.
> Todo39 IN_PROGRESS: stadia_gatt_input.hpp endpoint lifecycle + production
> StadiaController bind/consume/retire/publish APIs; all30 fixtures through lease.
> No radio/virtual attach implemented. Input/output/lifecycle must be serialized
> by future adapter. Raw HID ignored permanently after first GATT binding.
> Reviewer found generic3f neutralization and command-reply combo-remap bugs.
> FIXED via stadia_virtual_input.hpp: only full format30, generated21/30; reject
> other formats before base mutation. Bounded copy/remap/emit common injection.
> Native struct/offset static asserts. Follow-up reviewer found no issues.
> Mac and pinned Docker full tools/test-stadia.sh PASS after fixes. Host pub tests
> use explicit doubles, NOT real SDK controller/hardware certification.
> Build attempt1 foreground timed out; container later absent, no success proof.
> Attempt2 read-only source SDK mount failed sys/socket.h (BSD overlay missing).
> Corrected SDK stage follows pinned Makefile install recipe incl external/bsd:
> ~/.cache/missioncontrol/stadia-build-sdk/20261020T114634Z/.
> CURRENT detached build: mc-stadia-controller-build-complete-sdk-20261020,
> ID0cc99d3afc7e, no --rm, final MISSIONCONTROL_BUILD_EXIT marker. Source fixed
> before this launch. Read-only delegate39a22c27-b5b9-4c33-a364-440bbbc6bca0
> waits once and will wake parent. Do not poll/restart while active. Await result,
> verify actual artifact/compiler status, fix errors then complete39 only if pass.
> NO Switch deployment/radio/session/settings/IPS changes in this loop.
> Todo38 callback/owner identity remains blocked; researcher source_check hit
> HTTP429 usage_limit_reached; lane stopped. Direct GitHub644/658 reads add no
> missing callback identity. Do not map next UUID event to current ticket.
> Older notes saying no background work/next controller implementation superseded.
>
> Previous checkpoint — bounded HOGP session core implemented/tested; Switch adapter NOT yet wired.
> User explicitly asked to implement MissionControl and test live, then said let's go.
> Added mc_mitm/source/bluetooth_mitm/bluetooth/hogp_session.hpp: verified catalog,
> map, reference, subscription-ACK sequence; exact one-shot tickets/endpoints,
> 5s step/30s total setup deadlines, generation checks and route retirement.
> Added tests to hogp_report_tests.cpp including30 fixtures through full modelled
> session. Full macOS AND pinned Docker tools/test-stadia.sh pass; pinned AArch64
> C++ syntax check passes. Existing tracker designated partial initializer caused
> GCC -Werror failure; replaced with explicit zero-init lambda, preserving values.
> No module deployed or Switch operation. New core does NOT solve discovery or
> correlate otherwise unidentifiable SDK callbacks. Runtime integration blocked:
> discovery handles are documented, but BtmBleDataPath unknown byte+UUID and
> BleClientGattOperationInfo undocumented fields+UUID/data do not yet establish
> instance/owner identity. Earlier managed-path certainty was too strong.
> Docs tools/BLE-TRANSPORT.md record obligations; todo37 done,38 pending evidence.
> Do not label the whole module done or request unchanged scan as a new test.
> No background work running.
>
> Prior live Mac HID SUCCESS; approved live CoreBluetooth metadata test:1812 NOT exposed.
> Live archive ~/.cache/missioncontrol/stadia-host/live-gatt-metadata-20261020T110752Z/.
> One matching candidate, connect acknowledged,2 services visible,1812 absent for
> this client. No map/reference reads; no notifications/writes/scans/Switch ops.
> Exit1 capability negative, not crash. One connect/one cancel, terminal callback
> confirmed own app-reference cleanup. No claim OS HID link disconnected.
> Raw GATT unvalidated, not proof device lacks HID. Do not repeat unchanged or
> unpair/disconnect OS-managed controller as workaround. Older not-live notes
> below are superseded by this actual live result.
> Probe tools/stadia-host/build/stadia-gatt-capability 164616 bytes,
> SHA256 399e2f1db0a164fa85d3e821d0f268fc964e63c379312be5cf56994114a9a850.
> --connected-catalog requires explicit live-session approval. One named system-
> connected Stadia candidate (not authenticated VID/PID), no scans/fallback IDs.
> Reads services/map/2908 only, no report reads/subscriptions/writes/pair API.
> Production C++ bridge validates map/reference sizes. One owned CB connect/cancel;
> callback confirms retirement; 45s operation +8s cleanup, no retry/global reset.
> Full tools/test-stadia.sh passed incl sanitizer bridge and Swift offline model/
> CLI/privacy source contracts. These do NOT mock live CB delegate behavior.
> No Bluetooth manager initialized during this implementation; no Switch actions.
> Older 'next implement' notes below are superseded: next gate is live approval.
> Hostplan tools/STADIA-HOST-FIRST-PLAN.md; runner tools/stadia-host/.
> UserMacBluetoothON/Stadiaconnected, explicitly30secscopednativeHIDtest:
> 343validreports,340statechanges,0rejected,exit0,oneattached,no removal.
> UserconfirmedA+B,leftstickcircle,rightstickcircle,A,B; capturematchescombo+
> separatepress/releases andbothstickmovement. LastACTUALinputneutral+terminal
> neutral. AxesLX0..4095,LY0..4078,RX/RY0..4095. No repeatedcapture needed.
> Archive~/.cache/missioncontrol/stadia-host/live-hid-20261020T073936Z/.
> Nativebinary62216SHA b3f69b00cd29e9af8474b5e53c40ead0727466efb1a1819c075db9a39217407c.
> SAMEproductionC++report/map/router30fixturestatespassMac+DockerASan/UBSan;
> no externaldeps/transpilation. ModelledGATTfixtureisNOTliveGATTproof.
> IOHIDrunreadOS-managedconnectiononly,nonseizing:no pair/connect/output/
> Bluetoothsettings/Switchrequest;managerclosed,NOTphysicalOSlinkdisconnect.
> No reporthex/deviceIDs/addresses/credentialslogged;normalizedstatearchiveonly.
> Removal/disconnectbehaviorNOTlive-tested. Raw1812/2A4B/2908/2A4D accessUNKNOWN.
> NextIMPLEMENT/TESTboundedCoreBluetoothservice-capabilityprobeBEFOREasking
> anotherdeviceaction; do notrunonewithoutoperatorreadiness/explicitscope.
> No SwitchFTP/netloader/reboot/firmwaredump needed for currenthost work.
> macOSHIDvisibilitymaydifferfromiOS;don'tassumeCoreBluetooth1812blocked/exposed.
> DockerDesktopcannotdirectMacBT;usedforofflineC++replay/buildtests.
> SwitchstillP2module182784SHA60293ec9...008e9verifiedloadedafterHekate;93IPSINIboot
> preserved. NativeHIDscan0ads/clean1/1/no connect; discoverygateSTILLunresolved.
> Bothnativecodeexportsfailed;notprerequisitehostwork,no retries/keys/NAND.
> No jobs remain running. Section44. No unowneddisconnect/rawcancel/globalreset.

## 1. Project Goal & Current Status
- **Goal**: Implement reusable, safe BLE/HID-over-GATT (HOGP) gamepad support in MissionControl, with Google Stadia (`18d1:9400`) as the reference controller.
- **Hardware/Firmware**: Nintendo Switch HOS **21.2.0**, Atmosphère **1.11.2**, emuMMC (`CFW (EMUMMC)`).
- **Core Discovery Finding**:
  - Stadia BLE's merged Android advertising record contains standard HID Service (`0x1812`) in AD type `0x02` (Incomplete 16-bit Service UUIDs) at combined offset 31+. Scan-response placement is strongly suggested, but has not been independently proven from separate packet captures.
  - Our Android controls were primary-positive/response-only-negative for both UUID widths. This is observed placement dependence, NOT proof that all native Broadcom/Switch filters are intrinsically primary-only.
  - Direct connection to the known controller address is a candidate discovery bypass, NOT hardware-validated. BTM cache/address-type requirements and safe cancel/disconnect completion remain unproven.
- **Current Milestone**:
  - Reusable HOGP report router and report map parser implemented and validated (`hogp_report_router.hpp`, `hid_report_layout.hpp`).
  - Standalone connection guard and lease engine implemented and unit tested with ASan/UBSan (`ble_connection_lease.hpp`, `tests/ble_connection_lease_tests.cpp`).
  - Module-side guarded connection probe implemented with native command-65 tuple capture and bond blocking (`bluetooth_connection_probe.cpp`).
  - One Android managed connect/disconnect succeeded; pending cancellation cleanup did not confirm and coincided with slowdown. A later fresh-boot scan-only batch also preceded slowdown. These are diagnostics, NOT working Stadia/HOGP controller support; current focus is event/ownership regression instrumentation.
  - Clean ARM64 release build succeeds (`bash tools/build-switch.sh`).

---

## 2. Operator Workflow & Communication Protocols

Always maintain clear communication with the user regarding console operations. Follow these exact channels:

### A. Netloader (Live File & Executable Transfer)
- **Port**: `28280` on Switch IP (`192.168.1.146`).
- **Use Case**: Deploying and launching standalone test executables (such as `stadia-probe.nro`) directly to hbmenu over the network without rebooting.
- **How to Operate**:
  - Ensure hbmenu is active on the Switch with Netloader open (user presses `Y` in hbmenu).
  - Use `tools/netload-stadia.sh` or standard `nxlink`:
    ```bash
    nxlink -a 192.168.1.146 tools/stadia-probe/stadia-probe.nro
    ```
  - Probe communicates bidirectionally with the host broker via TCP.

### B. Persistent Probe Console Commands
- **Use Case**: Safe, controlled runtime telemetry, diagnostic execution, and non-destructive SD inspection/staging.
- **Architecture**:
  - Host broker runs locally: `python3 tools/stadia-console.py serve 192.168.1.146 --upload`
  - Control socket: `~/.cache/missioncontrol/stadia-console/control.sock`
  - Outbound TCP from Switch to host: port `28771` (telemetry/control) and port `28772` (guarded binary transfers).
- **Standard Commands**:
  ```bash
  # Check connection and diagnostic status
  python3 tools/stadia-console.py send status

  # Request synthetic advertisement control test
  python3 tools/stadia-console.py send control

  # Run scoped scan tests (never prints private MAC/keys)
  python3 tools/stadia-console.py send hid-complete
  python3 tools/stadia-console.py send hid-incomplete
  python3 tools/stadia-console.py send real-complete
  python3 tools/stadia-console.py send real-incomplete

  # Abort active operation or exit probe cleanly
  python3 tools/stadia-console.py send cancel
  python3 tools/stadia-console.py send quit

  # Guarded SD file transfer operations (restricted to live INI & staging)
  python3 tools/stadia-console.py file list <path>
  python3 tools/stadia-console.py file get <remote_path> <local_dest>
  python3 tools/stadia-console.py file hash <remote_path>
  python3 tools/stadia-console.py file put <local_file> <remote_path> --expected <sha256>
  ```

### C. FTP (Filesystem & Payload Transfers)
- **Port**: `5000` on Switch IP (`192.168.1.146`).
- **Use Case**: Reading logs, backing up SD card files, updating sysmodules, or staging Atmosphere files when the probe is not running.
- **Handling Quirks**:
  - Always `cwd` into a folder before calling `nlst()`; do not pass paths directly to `nlst()`.
  - Exclude dot-prefixed auxiliary files (`._*`).
  - If a write or rename return code is ambiguous, verify by reconnecting and reading back hashes; do not retry blindly.

### D. Console Reboots
- Ask the user before triggering or requesting reboots.
- Reboots must always target **CFW (EMUMMC)** via standard Atmosphère/HATS reboot mechanisms.
- Do not modify raw boot flags, keys, or global Bluetooth settings.

---

## 3. Safety Invariants & Guardrails
1. **Zero Exposure**: Never output private Bluetooth MAC addresses, encryption keys, pairing credentials, raw process memory dumps, or sensitive tokens.
2. **Preserve Configuration**: Protect all 93 baseline MissionControl IPS patches, existing controller configs, and stock boot flags.
3. **No Speculative Actions**: No unowned GATT calls, blind retries, or global Bluetooth filter clears (`command 60`).
4. **Guarded Leases**: Experimental attempts bind ARUID and exact native connect tuple; the module requests stop after ≤15s. Cancel IPC acceptance is NOT terminal completion. Native pending cancel uses the captured tuple; connected cleanup requires the live NRO to resolve an ARUID-qualified BTM handle. No guarantee of rollback after NRO exit, IPC stalls, ambiguity or hardware failure; unconfirmed cleanup requires operator recovery, not blind retries. The feature remains off by default until explicitly enabled.
5. **Clean Rollbacks**: When synthetic tests complete (e.g. Android nRF Connect), revert temporary advertising permissions and stop services.

---

## 4. Next Technical Steps
1. **Android Controlled Connection (hardware)**: Verify the full connection lease lifecycle (Arm → Connect → Cancel/Disconnect → Status verification) with `--connect` against an Android-advertised target before physical Stadia hardware. Record whether the reserved tuple is captured, whether `CONNECTED`/`LEASE RETIRED` is reached, and whether cleanup is confirmed.
2. **Stadia Direct Connect (future, gated)**: Only after controlled ownership/cancellation tests and explicit operator approval, test the known Stadia address. GATT discovery/CCCD/subscriptions are separate later authorization and implementation steps; do not issue speculative GATT or pairing requests.
3. **Virtual controller attach**: Feed decoded Stadia reports into `StadiaController` and the virtual-controller lifecycle, with disconnect/reconnect cleanup and multi-controller testing.

---

## 5. Progress Log — 2026-10-19 (probe connection client)

### Implemented offline
- `tools/stadia-probe/source/connection_client.hpp` (SDK-free, host tested):
  - Byte-layout `FindConnectionHandle` for `BtdrvBleConnectionInfo` (12-byte wire
    record); ignores the invalid handle `0xffffffff`, matches only the reserved
    target, never guesses.
  - `MakeLeaseToken` (nonzero, stable per lease) and `ClassifyLease` (numeric
    mirror of the module `State` enum; treats quarantine/ambiguous as final).
- `tools/stadia-probe/source/main.cpp`:
  - New one-shot `--connect` mode, also `--target ADDRESS --connect`; A-gated,
    PLUS cancels. It is separate from the scan `Probe` and never scans.
  - Sequence: read status (cmd 7) → require protocol 1 + `enabled=1` + clean
    lease → `btdevInitialize` → arm with the NRO's own ARUID (cmd 8) → managed
    connect once via `btdevConnectToGattServer` → bounded status polling (15 s
    connect + 20 s hold) → stop (cmd 9) → bounded cleanup (10 s).
  - Managed disconnect: resolves the owned handle from
    `btdevGetBleConnectionInfoList`, matches only the reserved target, disconnects
    once, then reports the result (cmd 10). A missing/ambiguous handle reports a
    failure instead of disconnecting anything; a managed-disconnect attempt is
    never retried. All failure paths log fail-closed and require a restart.
  - `--status-only` now also sends read-only command 7 and logs
    protocol/enabled/state/reason/armed/captured/terminal/managed-disconnect.
  - Probe version bumped to **0.6.8** (`nacptool` too).
- `tools/netload-stadia.sh`: `--connect` accepted as a real-controller mode
  (Mac Bluetooth must be OFF; no radio toggling). Launcher test cases added.
- `tools/check-stadia-scope.py`: added source-contract assertions for the client
  (commands 7/8/9/10, handle resolution before disconnect, no pairing/GATT calls,
  no native cancel from the NRO). Existing `Probe::Run` invariants still pass.
- Docs: probe README section for the one-shot mode, `tools/BLE-TRANSPORT.md`
  integration note, `HANDOFF.md` milestone/next-step updates.

### Verification
- `bash tools/test-stadia.sh` passes (all suites, ASan/UBSan, no hardware).
- `bash tools/build-switch.sh -C tools/stadia-probe` builds clean.
- `bash tools/build-switch.sh` builds the ARM64 module clean; the ELF contains
  `ArmBleConnectionProbe`, `GetBleConnectionProbeStatus`,
  `StopBleConnectionProbe`, and `CompleteBleManagedDisconnect`.

### Hardware runs (netloader, no connection attempted)
- Probe 0.6.7 `--status-only`: uploaded and ran. Live module reports **0.16.0**
  (`0x001000`), scan protocol 5, scan=1, bridge=1, filters clean
  (`added=12 removed=12 active=0 failed=0`), managed events total=31 (scan=7,
  errors=0), bridge copied=7 rejected=0. No Bluetooth request was sent.
- Probe 0.6.8 `--status-only` with the new read-only command 7:
  - `Connection probe status: IPC result 0x0000f601`
  - The installed 0.16.0 sysmodule does **not** expose commands 7–10; it predates
    the connection-lease IPC. The freshly built
    `mc_mitm/out/nintendo_nx_arm64_armv8a/release/mc_mitm.nsp` (180497 bytes)
    does contain them, but is not installed.
- Netloader operational note: a bare TCP probe (`nc`) of port 28280 **consumes
  hbmenu's one-shot netloader session** and produces `error getting name length:
  err=11` on the console. Never probe 28280 with `nc`; launch `nxlink` directly
  and read results with `docker logs <container>`. Re-arm netloader (Y) after any
  aborted/consumed session.

### Current blocker and next action
1. Start FTP on port **5000**. Back up first, then:
   - read/hash/download `/config/MissionControl/missioncontrol.ini`;
   - hash/download the installed
     `/atmosphere/contents/010000000000bd00/exefs.nsp`;
   - upload the new `mc_mitm.nsp` as `exefs.nsp`;
   - add only `experimental_ble_connection_probe=true` to the live INI (keep
     scan/bridge flags and every other setting), preserving all 93 IPS files.
2. Ask for a reboot to **CFW (EMUMMC)**; re-run `--status-only` and require
   `protocol=1 enabled=1 state=0 armed=0` before any connect.
3. Then, and only then, request explicit approval for one connection-only test
   against a **connectable BLE advertiser** at a known address (e.g. Android
   nRF Connect). Earlier Android approval covered advertisements, NOT initiated
   connections. No pairing or application GATT operations are authorized by that
   earlier approval. The module guards BTM's explicit CreateBond command; this
   is NOT proof that every native security/pairing path is blocked.

## 6. Follow-up safety review — probe 0.6.9 (offline)

Reviewed the 0.6.8 client before deployment and fixed:
- Wait/hold/cleanup loops now pump local PLUS/applet events. Previously they
  only slept, so the advertised PLUS cancellation was not serviced there.
- Time bounds use monotonic ticks rather than counting iterations; the hold
  ends when module status leaves Connected (including lease timeout).
- Handle lookup now takes the actual buffer size, rejects excessive count,
  and rejects multiple matching records, including repeated identical handles.
  Failed lookup leaves the caller's output unchanged. Added ASan/UBSan tests.
- Cleanup no longer treats quarantine alone as terminal: it observes for the
  bounded cleanup interval so a late owned success can request managed disconnect.
  Ambiguity still prevents all automatic disconnect attempts.

All offline suites, pinned ARM64 module/probe builds and `git diff --check`
passed; the final version-bumped 0.6.9 probe was rebuilt and suites rerun. No new
module/config has been transferred, no reboot requested, and no BLE connection
attempted during this review. Latest FTP port-5000 check returned connection
refused. Netloader was not probed (do not consume its one-shot session).

Next operator action: ask for FTP when the review/build is finished; back up and
verify the live module/INI and baseline IPS before any narrowly scoped module
replacement. Ask for reboot only after verified transfer. Netloader alone can
run the new NRO but cannot make an old running module expose the new IPC.

## 7. Verified FTP deployment — reboot pending

After the user enabled FTP, the candidate module and one INI opt-in were
transferred and read back byte-for-byte. This changes files on SD only; the
old module continues running until reboot.

- Backup directory:
  `~/.cache/missioncontrol/stadia-backups/connection-lease-20261019T182355Z/`
  (`manifest.json` records predeployment files, candidates, SD backups and
  post-transfer protection verification).
- Installed `/atmosphere/contents/010000000000bd00/exefs.nsp`:
  180497 bytes; SHA256
  `247619057a442223b32d75a38062183f03da85aacae384da0f8a523e61beee2a`.
- Installed `/config/MissionControl/missioncontrol.ini`:
  213 bytes; SHA256
  `ac49acdf1f80c226487da4dfc46c7b434472c5ae89c14b0cdbd9b852b39113da`.
  Only added `experimental_ble_connection_probe=true` inside `[bluetooth]`;
  removing that inserted line reproduces the exact original 174 bytes.
- Originals retained on SD at each original path plus suffix
  `.pre-lease-20261019T182355Z`, and in the host backup. No blind retries:
  staged payloads were verified first, then original-to-backup and stage-to-live
  renames verified. These separate renames are not power-loss atomic.
- All 93 baseline IPS hashes AND filename sets verified before and after;
  boot flag, mitm list, toolbox metadata and INI template unchanged.
- No new probe NRO deployed in this FTP operation. Local probe 0.6.9 is built
  for subsequent netloader launch. No connection/pairing/GATT attempted.

**Next action:** ask the user to reboot normally into **CFW (EMUMMC)** and open
hbmenu netloader. Launch probe 0.6.9 in `--status-only` mode and require connection
`protocol=1 enabled=1 state=0 armed=0 captured=0` before planning any connection.
Do not probe netloader port 28280 with a bare TCP connection. Check responsiveness
and stop if the previous menu-lag regression returns. Android connection tests
still require explicit authorization; do not infer it from FTP/reboot readiness.

## 8. Post-reboot status-only validation — passed

User rebooted and enabled netloader. Launched built probe **0.6.9** (335928 bytes)
with `bash tools/netload-stadia.sh 192.168.1.146 --status-only`; Docker output
container `ee83e83340225bf82cb551c2b07c40aa5a4bcaf9a7f22f8a49c085f7e73c7315`.
Physical A authorization completed; status output:

- HOS `0x00150200`; running MC `0.16.0` (`0x001000`).
- Scan protocol5, scan=1, bridge=1; added/removed/active/failed all0.
- Advertisement format=0 (not learned since reboot), bridge counters all0.
- Connection protocol1, enabled1, state0, reason0, armed0, captured0,
  ambiguous0, terminal0; connected/disconnected/managed-disconnect all0.
- Probe explicitly confirms **no btdev/btmu initialization or Bluetooth request**.

Probe is at `Finished / Press PLUS to exit`; the user must press PLUS and re-arm
netloader before another launch. No live BLE connection attempted. Next gate:
confirm menus remain responsive and obtain explicit permission for one controlled
Android connect/disconnect-only test (not pairing, service discovery or GATT).
Do not run the existing `--connect` launcher blindly: it reads the stored STADIA
address. Android test needs a separately prepared private target and suitable
radio preflight, without exposing its address in logs or Docker metadata.

## 9. Authorized Android control connection — discovery gate not yet passed

The user explicitly approved one Android BLE connect/disconnect-only test, with
no pairing or GATT attribute operations. Menus were reported healthy. Android
ADB is currently unavailable (`no devices/emulators found`); do not bypass its
screen lock or assume USB access. The user manually configured nRF Connect:
legacy/connectable/discoverable, primary UUID16 `1812` plus primary diagnostic
UUID128 `9f6dca74-69b7-4a35-9f01-9ab841ab5832`, Scan Response empty.

Implemented `--connect-control` (NRO-only, no module changes):
- Native UUID128 scoped discovery; collect all candidates, reject multiple or
  invalid addresses, rotation, errors or full/truncated batches.
- Keep selected address only in RAM, never load/export/overwrite Stadia target.
- Require successful scan stop and clean status, then a second fresh A before
  the single connection attempt; confirmation expires after 30s.
- Reject pre-existing target handles in this ARUID's connection list.
- Test launcher requires Mac Bluetooth OFF but does not inject the stored target
  for this new mode. Added selection tests, launcher tests and source guards.

**Hardware run 0.6.10:** Mac Bluetooth confirmed off. Launched via netloader
container `8b6f5784a9a590305ccdce9fc376875789703c40cd7377c0767bb3babfcc5d32`.
Native scan start/stop both succeeded. Its five-second selection window returned
`CONTROL TARGET UNCONFIRMED` (the old summary did not distinguish absent from
ambiguous). No lease arm, connection, pairing or attribute operation occurred;
probe finished and exited. Do not call this a failed connection trial.

**Follow-up 0.6.11 built, not launched:** archived native-UUID positive control
(`android-native128-primary-positive-hid-response-negative-0.6.4-20261019T164330Z`)
logged zero candidates at both tick0 and tick20 (5 seconds) before succeeding.
Therefore restored the previously used 30-second scan budget; no UUID/mode or
global settings change. Added batch count, maximum candidates, seen flag and
issue bitmask (1 invalid address, 2 multiple addresses, 4 untrusted batch), all
identity-free. Offline suites, ASan/UBSan selection tests and pinned probe build
pass. User was asked to re-arm netloader (Y); wait for readiness before relaunch.
The second A gate still prevents an automatic connection after discovery.

## 10. Android connection lifecycle validated once — 0.6.11

User re-enabled netloader; Mac Bluetooth preflight confirmed OFF. Run container
`0516edff33263f30e321efb7f111c16a42cac68dab3bf98f960a300ab058d8b2` completed:
- Native control scan: 575 batches, max_candidates1, seen1, issue_bits0,
  cancelled0. Stop scan IPC succeeded. Target stayed in RAM.
- User authorized the second fresh A gate.
- ARUID-qualified existing-connection query succeeded; lease arm and managed
  connect IPC succeeded. Exactly one native tuple captured, ambiguous0.
- Connected event1, state3. At the module's 15-second deadline, state4 reason2
  (TimedOut) requested one managed disconnect. Native pending cancellation was
  NOT exercised: cancel_requests0.
- Managed disconnect IPC succeeded; native disconnected event1, terminal1,
  state5, ambiguous0, managed_disconnect0; `LEASE RETIRED cleanly`.
- blocked_bond_requests0: no explicit CreateBond intercepted; not a claim about
  all native security or discovery activity.
- Completion-report IPC returned `0x00001759` because the native terminal event
  won the race and cleared `g_managed_disconnect` before the NRO's result report.
  Cleanup was already confirmed; do not retry the disconnect/connection.

Archived logs and anonymous result metadata:
`~/.cache/missioncontrol/stadia-backups/android-owned-connection-0.6.11-20261019T183837Z/`.
Lease is now ONE-SHOT RETIRED for this boot; a new trial needs reboot and explicit
approval. Stop Android advertising and exit probe with PLUS after this test.

### Acknowledgement race fixed OFFLINE, NOT deployed
Added SDK-free `ble_managed_disconnect_ack.hpp` and service bookkeeping: accept
one result report for an issued managed disconnect if the corresponding native
terminal event arrived first, without completing/mutating the lease again.
Unsolicited, duplicate and no-pending/no-terminal reports still fail; existing
session token/reserved validation is preserved. Host tests exercise both event/
report orders and failure paths. Full test suite, pinned ARM64 module build and
`git diff --check` pass. **The local rebuilt module now differs from the installed
module SHA `24761905...beee2a`; do not confuse local build with SD deployment.**
No second FTP transfer/reboot/connection was performed.

### User Android screenshot — sanitized observations only
Screenshot provided after test reports `DISCONNECTED / NOT BONDED`, a connected
entry, two rejected connection parameter updates (0x3b), then a successful update
(interval15ms, latency0, supervision timeout5000ms), and peer-terminated
connection status19 about14.5seconds after connection. This is consistent with
the Switch-side timeout-driven disconnect. It also says `Services discovered`
with CLIENT tab selected: **do not claim no service discovery occurred anywhere
in the stack**. Probe code makes no explicit attribute/discovery calls, but
native-stack/phone activity and the screenshot's connection role need clarifying.
Do not copy or echo the screenshot's Bluetooth address; only sanitized facts are
recorded here. This is Android lifecycle evidence, not Stadia/HOGP compatibility.

### User clarification
The nRF connection screen opened automatically when the Switch probed; the user
did not initiate a separate scanner connection. Android advertising was then
stopped. This supports the Switch-initiated lifecycle interpretation, without
establishing which native side performed the displayed service discovery.

## 11. Acknowledgement-fix module deployed — reboot pending

User enabled FTP after discussing the next deployment. Re-ran all offline tests,
pinned ARM64 module build and diff check, then backed up and verified the live
installation. **Only exefs.nsp was replaced; INI was not modified.**

- Host archive:
  `~/.cache/missioncontrol/stadia-backups/disconnect-ack-20261019T184845Z/`
- New SD module: 180610 bytes, SHA256
  `19e46802c51dd2b2582532d12910ffa7c2c18fa745a620c111e574466b6d5e06`.
- Previous module (180497 bytes, SHA `24761905...beee2a`) retained both in the
  host archive and on SD at
  `/atmosphere/contents/010000000000bd00/exefs.nsp.pre-ack-20261019T184845Z`.
- INI remains 213 bytes, SHA256
  `ac49acdf1f80c226487da4dfc46c7b434472c5ae89c14b0cdbd9b852b39113da`.
- All93 baseline IPS hashes AND filename sets plus boot flag, mitm list,
  toolbox metadata and INI template verified before/after, unchanged.
- Temporary upload readback verified before replacement; separate verified
  backup/live renames are recoverable, not power-loss atomic. No blind retries.
- No new NRO transferred, no connection/pairing/attribute operations attempted.

Next: request normal **CFW (EMUMMC)** reboot, confirm responsive menus, ask for
netloader and run probe0.6.11 `--status-only` first. Require protocol1 enabled1
idle/unarmed/captured0. Android should remain stopped; do NOT automatically
repeat `--connect-control`. The next experimental design is a separately
approved pending-connect cancellation test; no such test has been implemented
or run yet. Before reboot the old one-shot lease remains retired in RAM.

## 12. Post-acknowledgement-deployment reboot: status-only passed

User reported ready after reboot/netloader request. Launched probe0.6.11
`--status-only` via container
`6d91f2cbb24b8bd0f4b679b158ad106286044f6bcb0fe6f3a4b061f4e99aa76f`;
physical A authorization completed. Observed:
- Running MC0.16.0, HOS21.2.0, scan protocol5 scan1 bridge1.
- All scan/filter/event/bridge counters zero; learned format0 (reboot reset).
- Connection protocol1 enabled1 state0 reason0 armed0 captured0 ambiguous0
  terminal0 connected_events0 disconnected_events0 managed_disconnect0.
- Explicit status-only confirmation: no btdev/btmu initialization or Bluetooth
  request sent. Probe at `Finished / Press PLUS to exit`.

This confirms a clean post-reboot diagnostic state, NOT a runtime exercise of
the acknowledgement race fix. Do not report that fix hardware-validated yet.
Android remains stopped; no new connection approval or attempt in this step.
Next is offline design of a deterministic pending-connect cancellation test:
use only the explicitly selected Android control, never a fabricated/unowned
address. A contemplated setup is scan/cache the unique RAM-only candidate,
stop scanning successfully, ask the operator to stop advertising and separately
authorize an immediate-stop connection/cancel trial. Address/cache behavior,
terminal-event semantics and BTM bookkeeping remain unproven. Obtain explicit
approval before such a trial; keep late-success managed cleanup and no-retry
rules. At this status-only checkpoint no such mode had been implemented or launched.

## 13. Pending-cancellation probe prepared OFFLINE — 0.6.12

After the user agreed to offline preparation, implemented NRO-only
`--cancel-control`; **not launched and no new hardware action authorized yet**.
No module/INI changes or additional FTP/reboot needed for this mode.

- Uses the same native diagnostic UUID128 30-second RAM-only selection and
  successful scan-stop requirements. Cannot use the stored Stadia address.
- Second physical gate explicitly asks operator to STOP Android advertising
  first, leave phone Bluetooth on, then release/press fresh A within30s. The
  phone toggle cannot be verified by this probe; it relies on operator assent.
- One managed connect IPC followed immediately by one stop-intent IPC, without
  polling/waiting for connection first. The module alone chooses owned tuple
  cancel vs managed disconnect vs no action. No invented/unowned target.
- Stop-intent is now sent once even if the subsequent status query fails;
  destructor/cleanup never blindly retries the request.
- Existing bounded cleanup still handles late success through a live owned
  BTM handle and never native-id disconnect. Last diagnostic ARUID-qualified
  target-presence query reports ABSENT/PRESENT/UNKNOWN without addresses.
- SDK-free cancellation classifier distinguishes NOT EXERCISED, terminal after
  a cancel request, late-success managed cleanup, and UNCONFIRMED. Counter
  mismatch, action errors, residual/unknown handle, ambiguity and missing
  terminal cannot become a cancellation pass. Handle absence is not proof of
  cleared BTM pending bookkeeping; a matching terminal after cancel does not
  establish causality by itself.
- Polling budgets are bounded, but synchronous SDK IPC may block; no guarantee
  an immediate stop can execute while the managed connect call is stalled.
- Added host tests for handle-presence bounds and all evidence failure modes,
  launcher Mac-Bluetooth-OFF preflight tests and source guards enforcing no
  stored-target injection and immediate stop ordering. Full suites including
  ASan/UBSan, pinned ARM64 probe build and diff check pass.

Next operator gate: ask explicit approval for **one** Android pending-cancellation
trial, explaining uncertain cleanup requires normal reboot and no blind retries.
Only after approval ask for same primary HID16+UUID128 advertiser and netloader.
At second A prompt STOP the advertiser; no pairing or application GATT operations.
Android is currently stopped per user; do not assume it was restarted. No further
connection attempt has occurred since the single 0.6.11 lifecycle success.

### 0.6.12 launch attempt: netloader transport failed before execution
User explicitly approved one cancellation trial and reported advertiser/netloader
up. Mac Bluetooth preflight confirmed OFF. Launch container
`bddbdfb66981f0e06c00c860b8aad18590aa341fe3cf8a864f0200bc501b0045`
returned only `Connection to 192.168.1.146 failed`. No upload/probe startup, scan,
lease arm or BLE connection request observed. This is NOT a cancellation-test
result. Stop and ask user to re-arm hbmenu netloader (Y); do not blindly retry,
probe28280 with bareTCP, change IP/settings speculatively or switch tooling.

### User-requested retry: zero candidates; cancellation NOT exercised
User explicitly requested retry. Mac Bluetooth OFF preflight passed; probe0.6.12
(340024bytes) launched via container
`5b213170314211b1dc624318ed2ca8fe2c62a91ec88053eee53dadd5df51e653`.
Native UUID128 selection ran30seconds: batches574, max_candidates0, seen0,
issue_bits0, cancelled0. Scan start and stop both returned success. Probe logged
`CONTROL TARGET UNCONFIRMED` and finished; no lease arm, connect or cancel.
Archive:
`~/.cache/missioncontrol/stadia-backups/android-cancel-selection-negative-0.6.12-20261019T190140Z/`.

Asked user to verify advertiser is actively running (not merely saved) and both
UUID16 and diagnostic UUID128 remain in PRIMARY data. If running, ask for a
configuration screenshot cropped to omit Bluetooth addresses. Do not infer UUID
byte-order regressions, change global settings, or retry before setup is checked.
The one authorized BLE cancellation attempt has not been exercised; native scan
was stopped cleanly, so no lease-based reboot requirement arose from that scan-only run.

## 14. Pending cancel ACTUALLY exercised — completion UNCONFIRMED; reboot required

User explicitly requested another rescan. Same settings/UUID/probe0.6.12; Mac
Bluetooth confirmed OFF. Container
`92977b8ba77bd4e537e9a3bf9c884853c4598f8dfadc68722ee3ed18c2503308`:
- Native30second scan: batches574, max_candidates1, seen1, issue_bits0,
  cancelled0. Scan stop IPC succeeded.
- Second physical prompt instructed STOP Android advertising; user pressed fresh
  A. Existing owned-handle query, arm, connect and immediate-stop IPCs succeeded.
- Module captured1 exact request; cancel_requests1, last_result0, ambiguous0.
  No connected/disconnected events; no managed-disconnect request.
- After10second cleanup observation: state4 (Stopping), reason1 (Stopped),
  armed1, terminal0. ARUID-qualified target handle query reported ABSENT.
- Probe correctly reported `LEASE CLEANUP UNCONFIRMED` and
  `CANCELLATION CLEANUP UNCONFIRMED`. **Do NOT call this a cancellation pass or
  infer terminal completion from IPC success/managed-handle absence.**

Archive:
`~/.cache/missioncontrol/stadia-backups/android-pending-cancel-unconfirmed-0.6.12-20261019T190310Z/`.

User was told immediately: keep Android advertising stopped, press PLUS, reboot
normally into **CFW (EMUMMC)**, no connection retry. Await reboot confirmation;
do not assume recovery happened. One authorized cancellation trial is consumed.
No additional pending cancel, unregister, client registration, global filter
clear/reset, pairing, guessed GATT call, module/config change or speculative
scan-mode override was performed. The acknowledgement-race fix was NOT exercised
because there was no connected/managed-disconnect event.

Next research: determine whether successful native command66 is expected to
produce a terminal ClientConnection event at all and how BTM pending bookkeeping
is retired. A missing event is an unresolved completion contract, not proof the
radio is still connecting or proof cancellation failed. Do not merely extend
polling or mark a lease terminal on IPC acceptance. No Stadia attempt until
ownership/completion evidence is adequate; any new hardware trial needs approval.

## 15. CRITICAL: user reports post-cancellation system sluggishness

While rebooting after the unconfirmed pending-cancel trial, the user reported
that **after the probe the system became unresponsive/sluggish and reaching the
reboot menu took a while**. This is a serious live regression. Do not describe
this trial as safe/healthy just because command66 returned success. Exact cause
is unproven: native cancel, BTM bookkeeping, module integration/session cleanup
or another path may be involved. Do not infer it is the same as earlier0.4 lag.

**STOP ALL LIVE BLE TESTS AND PROBE LAUNCHES.** User is rebooting; wait for
confirmation the reboot finished and HOME menus are responsive. Do not initiate
FTP, netloader, radio/status probes or additional hardware diagnostics while
recovery is in progress. No more cancellation trial, Stadia connection, global
reset/filter clear, scan-mode override, or speculative GATT.

Recommended next recovery (proposed, NOT yet approved/transferred): restore the
verified pre-CONNECTION module plus exact matching INI from
`~/.cache/missioncontrol/stadia-backups/connection-lease-20261019T182355Z/`:
- exefs.nsp175718 bytes, SHA
  `2bf765b63805db0d2bb21f9eff5caf60e4ea85b77fbae4a1d1cf031c051d7043`.
- missioncontrol.ini174 bytes, SHA
  `90fea945cd3775eee79f74d88be313bc452fe145a31fa0db1f95b916041d3d23`.
This disables the new connection integration by reverting its module and opt-in,
while retaining the previously tested scan-only experiment and existing settings.
For full stable HATS rollback, a separate verified standard module backup also
exists; ask the user before selecting/changing recovery scope.

Before any recovery transfer: ask approval and FTP readiness after responsiveness
check; back up current live files; verify all93 baseline IPS and boot metadata;
replace only approved module/INI with readback and retained backups; request
normal CFW EMUMMC reboot afterward. Never reinstall HATS or alter patches/boot
flags. Offline code/evidence investigation may continue, but hardware testing
remains paused even if reboot restores responsiveness, unless the operator explicitly approves a narrower safe diagnostic scope.

## 16. Reboot recovery confirmed; local containment and offline review

User confirmed HOME menus responsive after reboot. No rollback or SD transfer
was approved/performed; do not imply the installed module or INI changed.
Reviewed cancellation/forwarding/session teardown offline. Structural findings
and limits are in `tools/stadia-probe/REGRESSION.md` (latest incident above the
separate historical0.4 incident): mixed BTM/native ownership, shared Bluetooth
event-thread acknowledgement wait, synchronous operation-lock exposure and
unverified managed-session teardown. None is a proven cause of the slowdown.
Public driver documentation supplies no pending-cancel callback guarantee; its
internal direct-flag caveat is not verified for the exact21.2.0 binary. A BTM
page-answer request failed and contributed no evidence. No Switch IPC was used
for this investigation.

Local probe0.6.13 now has `ConnectionDiagnosticsPaused=true` before any
connection-mode initialization/selection/arm. Host launcher blocks `--connect`,
`--connect-control`, `--cancel-control` before preflight/Docker/upload. There is
no CLI/env bypass. This affects LOCAL tools/builds only, not the installed module
or old NROs. Host tests cover both launch rejection and independent NRO gate.

## 17. User-approved scan-only batch; upload failed before execution

User subsequently enabled advertiser/netloader and explicitly encouraged more
progress/batched testing. Agreed scope: read-only before/after status plus three
SCOPED scan controls; do NOT interpret this as permission to replay raw cancel or
unverified GATT/security operations. Pending-cancel path remains blocked.

Implemented `--inspect-control` in probe0.6.13 (NRO only):
- Read scan and connection status; require clean scan/bridge state and entirely
  idle/unused lease/worker before any scan.
- Native UUID128 calibration, complete-HID16 private-filter control, then
  incomplete-HID16 control. Existing Probe code only; no address selection,
  target-file access, lease arm, connect/cancel or pairing/GATT attribute calls.
- Require fresh HID evidence for the latter controls, successful managed scan
  stop, active0/failed0/balanced adds/removes and idle lease between every pass.
  Stop on first failed/cancelled/unconfirmed stage.
- Mac Bluetooth OFF preflight; Android dualUUID advertiser stays ON throughout.
  One physical fresh A gate; PLUS aborts. No module/INI update.
- Full suites including ASan/UBSan, launch/source guards, pinned probe build and
  diff check passed before launch.

Launch container
`588dc3235472d64d827020482cde608070d36b41820108a0df5c0f25af1c41cc`:
Mac Bluetooth OFF confirmed, but nxlink reported only
`Connection to 192.168.1.146 failed`. NO upload/probe startup or Switch scan/IPC
observed. Stop and ask user to re-arm hbmenu netloader Y; no automatic retries,
TCP port probes or tooling switch. Do not report the batch as tested on hardware.
Current installed module still acknowledgement-fix SHA19e46802...d5e06, INI
ac49acdf...13da; current local probe is0.6.13. No further connection since the
single unconfirmed cancellation trial before the confirmed recovery reboot.

### Second scan-only upload failure — verify netloader screen/IP before retry
User again reported advertiser/netloader on. Launcher --inspect-control passed
Mac Bluetooth OFF preflight; container
`8f6b68918eb9264c888053b8ede739397799d19caa4225bd792c6cf2350744de`
reported `Connection to 192.168.1.146 failed`, exited1. No probe start/upload or
Switch scan/connection observed. Host ICMP ping to192.168.1.146 succeeded2/2
(~4.5–4.9ms); that shows the IP responds, not that netloader is listening or that
it is necessarily still the Switch. No separate TCP probe of28280 was performed.
Asked operator for IP shown ON the netloader screen; stop repeated uploads until
screen/IP is confirmed. Do not change network settings or expand remote tooling.

## 18. hbmenu restart restored upload; scan batch refused non-idle state

User restarted hbmenu and explicitly requested retry. Container
`02e99713eececbd2eea3676228a6f3c1a53bee2f5b37be17dffa33f46c08116f`
successfully uploaded probe0.6.13 (335928bytes), physical A completed. Scan
status protocol5 was readable, then connection status IPC succeeded but
`Inspection connection state: idle=0`; the batch correctly aborted before any
scan or connection API. 0.6.13 did not print which field failed the strict idle
check (state/armed/captured/ambiguity/terminal/worker/events/actions/protocol).
Do NOT equate idle0 with proven ongoing radio activity or claim exact cause.
Archive:
`~/.cache/missioncontrol/stadia-backups/scan-inspection-idle-guard-0.6.13-20261019T192427Z/`.

Prepared probe0.6.14 OFFLINE: adds exact identity-free connection status counters
in status-only and the inspection guard (worker, cancel/disconnect counts, raw
result, protocol and lease fields). No guard was relaxed or bypassed; no new
Bluetooth operation was added. All tests, pinned probe build and diff check pass.
Not uploaded yet. Next: ask user PLUS to exit and Y to reopen netloader, then run
**--status-only**0.6.14 to identify the failing field. No advertiser needed.
Restarting hbmenu does not reset a sysmodule lease; do not assume a complete
system reboot occurred or accuse the user of not rebooting. Verify status before
requesting more recovery or changing files. No live scan batch had yet run at this checkpoint.

## 19. Retained cancellation lease identified; full reboot then verified clean

Probe0.6.14 status-only container
`126460ae31c9543648ed8e88680df3070e13ab074705d23fd92339398db1e275`
identified the failed idle fields: state4, reason2(TimedOut), armed1, captured1,
worker1, cancels1; connected/disconnected/terminal/ambiguity0, last_result0.
All scan/filter counters0, learned format2. No new Bluetooth requests. This
retained per-boot state is consistent with the prior cancellation trial; do not
claim it proves the radio is still connecting. Requested a full console restart
into CFW EMUMMC, not merely hbmenu restart.
Archive:
`~/.cache/missioncontrol/stadia-backups/retained-cancel-lease-status-0.6.14-20261019T192647Z/`.

User initially said netloader/advertiser on, then interrupted with **wait,
rebooted now, netloader on**. The earlier status upload container
`fdc8287f0829d1c5737f005c264c532b2d364b12fc6fe9bc94499847c687d616`
had failed before execution; it is NOT a post-reboot diagnostic result.

Fresh post-reboot status-only container
`0e24ece7e2c9fd9ca423286635fee2bc4b5329ac09ad47cbc786b6dfc6e1867f`
successfully ran0.6.14 after physical A:
- HOS21.2.0, MC0.16.0, scan protocol5 scan1 bridge1.
- Scan/filter/event/bridge counters all0, format0(reboot reset).
- Connection protocol1 enabled1 state0 reason0 armed0 captured0 ambiguous0
  terminal0 worker0 cancels0 disconnect_requests0 connected/disconnected0,
  last_result0. **Per-boot lease reset now actually verified.**
- Explicit no btdev/btmu initialization or Bluetooth request.
Archive:
`~/.cache/missioncontrol/stadia-backups/full-reboot-clean-status-0.6.14-20261019T192811Z/`.

Current probe is Finished/PLUS to exit. Next approved action is scan-only
`--inspect-control` after user re-arms Y; Android bothUUID primary advertiser
should stay ON, Mac Bluetooth OFF preflight. No connection-mode unpause, no
raw cancel replay. The clean reboot is recovery evidence, not cancellation
safety/root-cause validation. No live scan-only batch had run at this checkpoint.

## 20. Scan-only batch reproduced reported sidebar/power-menu slowdown

User armed netloader/advertiser; probe0.6.14 --inspect-control container
`3b5df99f5f34eb953701a60b3b0fb1985bfb3aafa0a0f767d9702dd3f4a35769`:
- Fresh baseline all scan+connection counters0, format0, lease/worker0.
- Stage1 nativeUUID128 positive, stopped successfully, learned format2.
- Stage2 private completeHID16: managed candidate0; driver_ads3 driver_hid_ads3,
  copied3 no_space0/unvalidated0/rejected0. Added1 removed1 active0 failed0,
  native stop success. Lease, worker, events and actions remain0/idle.
- Aborted at stage2 as designed; stage3 incompleteHID16 NOT RUN.
- DualUUID primary advertiser was intended: diagnostic128 plus1812, response
  empty. Existing128 list may affect interpretation of appended alias records;
  this is an unproven parser hypothesis, not an established HOS21.2 bug.
Archive:
`~/.cache/missioncontrol/stadia-backups/scan-batch-dual-uuid-hid-delivery-negative-0.6.14-20261019T193147Z/`.

User then reported mainHOME okay but HOME sidebar/power options slow. Explicitly
asked **do not reboot yet**, run diagnostic first. This fresh-boot scan-only
occurrence shows our armed connection lease/raw cancel/worker are NOT required
for every reported slowdown; do not continue framing it as cancellation only.

Built0.6.15 adding --inspect-hid-control (skip native128 calibration, require
already learned format) for a proposed HID-only-primary comparison. Asked user
to remove only the longUUID and report responsiveness. The comparison was NOT
RUN and advertiser changes were NOT confirmed before new slowdown report.
0.6.15 was uploaded ONLY --status-only in container
`0ffdb8758f0cb9b72a50a901c9a47423d342bb3f43caa8a95b9115d3afb62a85`.
Symptomatic read returned same1/1/0/0 filters, format2, copied3, all lease/worker/
cancel counts0. No btdev/btmu init or radio request.
Archive `scan-sidebar-slowdown-status-0.6.15-20261019T194617Z/` under backup root.

## 21. Broader diagnostic approval; read-only timing and unexpected nRF report

User explicitly authorized scans/connections/helpful tests, accepts reboot for
recovery, prefers autonomous progress and to be contacted only for needed input
or problems. This is permission for purposeful staged testing, NOT a reason to
blindly replay unresolved raw pending cancel or mix conditions without evidence.
No installed module/INI/IPS/boot settings changed.

Built/tested local probe0.6.16:
- --service-latency: existing MC status-only, timed MC cmd6/7, btmInitialize
  (SDK SM service open only), documented btmGetState cmd0, then btmExit closes
  only this session. No btdev/btmu/btdrv init, scan/arm/connect/cancel/pair/GATT.
- BEGIN precedes each synchronous call. No retry/worker/claimed timeout: an IPC
  may block the NRO. SDK btm.c lines10-18 and151-155 checked.
- All scan modes now paused at launcher before preflight/upload and independently
  in Probe::Run after the status-only branch. Connection gate remains true.
  This is temporary containment/diagnostic discipline, not operator refusal.
- Host/ASan/UBSan suites, source guards, fake launcher and pinned ARM64 build/
  diff check pass. Built NRO331832bytes. Tests do not prove native lifecycle.

User requested continue. --service-latency container
`8cefb5677f8459b640b0e8394b80a74a9a2c841de84f1d6504d66a17d94888cc`:
MCcmd6 275us, MCcmd7 472us, SMopenBTM358us, BTMGetState281us, all result0;
state6 = Initialized per SDK. **All scan/filter counters now0, format0**, unlike
symptomatic0.6.15. Cannot call this a symptomatic timing capture until operator
confirms intervening reboot/reset and whether menu slowdown persists.
Archive `mc-btm-service-timing-0.6.16-20261019T194617Z/` under backup root.

Asked that confirmation. User instead reported probe okay but **nRF noticed a
new connection**, netloader/advertiser on. This is unconfirmed unexpected native/
other-client activity, not an observed probe-owned connect. Do NOT dismiss it or
claim no connection/GATT activity occurred merely because our code sends no
connect. Asked user stop advertising, preserve nRF log, provide time/connection
log section with address hidden, and current sidebar/power responsiveness; no
reboot yet and no new scan/connect until understood. Do not disconnect an
unowned handle or guess raw commands. Current source/NRO0.6.16, installed module
still SHA19e46802...d5e06 and same enabled INI.

Two offline read-only audits were launched (project instruction authorizes
independent parallel analysis), no hardware/file edits delegated:
- ffe3667b-d0de-45b shared event progress completed. Verified shared BLE callback
  waits for GetEventInfo acknowledgement on event thread; pre-existing design
  can stall shared Core/HID progression if native consumer stops draining.
  Actual wait duration unknown. MCstatus responsiveness doesn't prove that
  event thread advances. Do not adopt agent's inaccurate inference that zero
  lease counters prove redirect-flag state, or that connection guard owns the
  redirect consumer. Instrumentation proposal only, no behavior/ack changes.
- 07283ce7-b046-42a advertisement-copy/delivery audit completed then corrected:
  current scan-pause source was added AFTER0.6.14 capture, so no contradiction;
 32byte UUID lists exceed29byte payload and are rejected, so no multi-entry-list
  bug. Multiple separate type7 records remain an UNPROVEN BTM parser hypothesis.
  No confirmed memory/record-bounds defect found. Do not implement speculative
  repairs based on the withdrawn claims.

## 22. Diagnostic-only module deployed/verified; activation pending

User explicitly offered FTP for module debug/capability additions. Explained
observational-only event metrics, no native behavior changes; user started FTP.
Built and reviewed new files:
`ble_event_progress.hpp`, `bluetooth_event_progress.{hpp,cpp}` and tests.
MCcmd11 protocol1 returns224byte trivial POD (three64byte lane snapshots plus
32byte header). Core/HID/BLE hooks record read/lock, processing, consumer-wait,
idle, phase start tick/type, dispatch/read/wait-return counts and last/max wait.
All original native calls/signals/waits retain exact ordering (source guards).
Existing command65 hook counts attempts BEFORE raw passthrough, including BTM
unreserved/unarmed attempts; no addresses/ARUID/handles/tuples/payloads/keys.
Scope is the existing intercepted path, NOT all possible Bluetooth activity.
No new btdrv IDs, radio changes, fake acks, event dropping, timeouts or retries.

Independent review14f97dc6-ab4d-498 found snapshot type store outside phase
sequence; fixed by bracketing type+phase+tick in one generation and extended
concurrent test to verify correlated type/phase/tick. All host ASan/UBSan suites,
concurrency, source/launcher guards, pinned ARM64 module+probe builds and diff
check passed AFTER fix. Initial CMIF POD constraint failure was fixed offline by
using trivial wire structs; no ABI force-enable or suppressed compiler checks.

Candidate/archive:
`~/.cache/missioncontrol/stadia-backups/event-progress-module-20261019T201407Z/`
- candidate-exefs.nsp181940bytes SHA256
  `58b68e85e08793bbba64f825093386c4d78b3e6c4027326aaea8eaa6d7590933`.
- stadia-ble-probe-0.6.17.nro335928bytes SHA256
  `08d70f231ad6b4aeb42503ca1290bfa8dad2753340a7b4bdb3a00f8153852c49`.
- before/ mirrors all99 scoped live files; manifest has pre/post digests,
  filename-set checks, candidate/staged/live/backup and reboot pending state.

FTP steps completed successfully, no ambiguous acknowledgements:
1. Read-only preflight compared current module180610bytes SHA19e46802...d5e06,
   INI213bytes SHAac49acdf...13da,93 IPS SHA/size+filename sets and protected
   metadata to prior verified manifest;99 files backed up locally.
2. Wrote only `exefs.nsp.new-event-progress-20261019T201407Z`; readback verified.
3. Live->`exefs.nsp.pre-event-progress-20261019T201407Z`; original SD backup
   hash verified. Then staged->live, new live hash verified. Separate renames
   are recoverable, NOT power-loss atomic; user told not to reboot mid-transfer.
4. Reconnected and rechecked all99 scoped files,93IPS filename sets, original
   backup and stage absent. INI/mitm.lst/toolbox.json/boot2.flag/template all
   byte-for-byte unchanged. NO NRO SD upload, other writes, rollback or reboot.

**Current installed-on-SD module is new SHA58b68...90933, not19e468...d5e06.**
Running process still old until user reboot. Told user safe to close FTP and
restart normally CFW EMUMMC, advertiser OFF, then hbmenuY/netloader ready.
Next launch local0.6.17 --event-progress (12 read-only MC samples over~11sec,
PLUS pumping between calls, no new native Bluetooth requests) and require cmd11
protocol1 baseline. --status-only also prints one snapshot; --service-latency
prints progress before/after documented BTM GetState. No IPC timeout guarantee.
All radio-changing local modes remain independently paused host+NRO until a
specific instrumented experiment is deliberately reviewed/unpaused. No actual
0.6.17 hardware run had occurred at that checkpoint. No current confirmation of
nRF log details or whether service timing followed another reboot; preserve
those uncertainties.

## 23. First cmd11 runtime check failed: oversized inline HIPC reply; fix awaiting FTP

User reported rebooted, netloader/advertiser on. Asked advertiser OFF for baseline
and launched0.6.17 --event-progress container
`575beb8506b5853932b7476428c3c093f5bdbec34c385b0b7bbddef44b98f0a3`.
Cmd6/7 status readable: all scan/lease/worker/action counters0, format0. Cmd11
failed0x0000e801 twice (status baseline then first sample), then stopped; NO
native Bluetooth requests/scan/connection. User said done then netloader back
listening. Advertiser-off instruction given; exact completion timing unclear.

Important: our old error text wrongly implied cmd11 unavailable/missing module.
Actual result is module1 description116 = kernel InvalidCombination, per pinned
`svc_results.hpp`; it does NOT establish unknown command or failed deployment.
Found a concrete transport defect: SF PrepareForReply encodes
8byteHIPCheader +16padding +16CMIFheader +224data =264bytes, exceeding
`hipc::TlsMessageBufferSize=0x100`. Initial POD size/concurrency tests missed the
FULL IPC envelope. Parent acknowledged this to user. No native-event/lifecycle
fix was inferred from this error.

Local correction (built/tested, NOT uploaded/deployed):
- ProtocolVersion2, same224byte status, MCcmd11 OutMapAliasBuffer with exact-size
  and non-null bounds; libnx client supplies Out|HipcMapAlias descriptor.
- Raw reply contains no POD (40byte envelope); all metrics transferred in buffer.
- No Btdrv/observer/wait/signal/guard/radio behavior changes.
- SDK-free CopyStatus tests short/oversize/null outputs leave sentinels intact,
  unaligned exact-size roundtrip passes. Regression asserts old inline264>256,
  empty-inline40<=256, source guards tie SDK formula/TLSsize to descriptor setup.
- Client errors log numeric module/description, no false missing-command claim.
- Full hostASan/UBSan/concurrency/launcher/scope suites pass. Module ARM build
  succeeded; combined invocation timed out AFTER probe also finished. Confirmed
  no Docker build left running, then reran probe build to capturedsuccess + diff
  check. No unresolved build error.

Candidate/archive:
`~/.cache/missioncontrol/stadia-backups/event-buffer-fix-20261019T203742Z/`
- candidate-exefs.nsp181763bytes SHA256
  `531f07c6d9db8f51b1ff2532e0ba3d104130cb3552d85106893998c140cf131d`.
- stadia-ble-probe-0.6.18.nro335928bytes SHA256
  `3314296a26a5594e86a63af437bb7fd032d3c13899b2283406069a7effd6ea26`.
- failed-inline-cmd11-0.6.17.log retains exact failed run, no identities.

Told user leave netloader waiting while inspecting, then switch to FTP for
corrected module update. **Await user FTP ready**; do not connect to presumed old
FTP server after reboot. Next deployment same proven module-only workflow:
use section22 manifest POSTDEPLOY files as expected live99file set (module now
58b68... not19e468...), back up current module/INI+protectedfiles host, verify93
IPS filenames/hashes and metadata, staged candidate/readback, live->unique SD
backup then stage->live (NOT atomic), postverifyall99 unchanged except module.
No blind write/rename retries; reconnect/readback on ambiguity. No INI/IPS/NRO/
boot writes. Then user normalCFWEMUMMC reboot, advertiserOFF, netloaderY; first
0.6.18 --event-progress must actually validate buffered protocol2 on hardware.
All radio modes still locally paused until this diagnostic works and baseline
is captured. User broadly approves useful staged tests; keep burden low but no
unresolved raw pending-cancel replay or unowned cleanup.

## 24. Buffered diagnostic module deployed/verified; user activation pending

User enabled FTP again. Verified current181940byte58b68... module and same
213byteINI,93IPS hashes/filename sets and protected metadata against section22
manifest postdeploy99files. Saved all99 before files to the new archive's
before/ tree. Candidate181763byteSHA531f07c6...40cf131d verified locally.

Deployment completed without ambiguous FTP acknowledgements:
- Staged `/atmosphere/contents/010000000000bd00/exefs.nsp.new-event-buffer-20261019T203742Z`, readback hash verified.
- Renamed live to `exefs.nsp.pre-event-buffer-20261019T203742Z`, old181940byte
 58b68... backup readback verified. Then staged->live; new181763byte531f07...
  hash verified. Separate renames are NOT power-loss atomic; user warned not
  to reboot until final verification. No blind retries or deletes.
- Reconnected and checked all99 expected files (module updated, others exact),
  all93IPS filename sets/hashes, backup stilloriginal, stagednameabsent.
  INI/mitm.lst/toolbox.json/boot2.flag/template all unchanged.
- NO NRO SD upload, config/IPS/boot changes, rollback or assistant reboot.

Authoritative archive/manifest:
`~/.cache/missioncontrol/stadia-backups/event-buffer-fix-20261019T203742Z/`;
state DEPLOYED AND POSTDEPLOY VERIFIED; USER REBOOT PENDING. Includes old58b68...
module host backup, candidate531f07..., local0.6.18NRO SHA3314296a...fd6ea26,
failed0.6.17 log, pre/post digests and protection flags.

Told user safe to closeFTP, restart normallyCFWEMUMMC, advertiser OFF, netloaderY.
**Await user readiness; no hardware probe since deployment.** First next action
`bash tools/netload-stadia.sh 192.168.1.146 --event-progress` local0.6.18, freshA,
12 read-only cmd11 buffered snapshots. Must confirm protocol2, clean scan/lease
baseline, event lane progress/ages and passthrough attempt counts; do not infer
native activity absent from ownedlease0. New module is not a slowdown remedy;
its transport and scheduling observations still require real validation. All
radio-changing modes remain locally gated. Todo28 inprogress until real buffered
IPC works; todo22 pending blockedby28. No useful background agents running.





## 25. Buffered cmd11 validated on hardware; user requests direct Stadia focus

After user confirmed activation reboot/netloader,0.6.18 --event-progress container
`6b4999bef351aa111c0c482e01dfd7214b7ec6c97030be9881228e4fded1fd6e`
returned initial + ALL12 protocol2 snapshots successfully. No IPC errors. All
scan/lease/worker/action/hook counters0, format0. Core/HIDidle0dispatches;
BLEidle9begun/finished/waits/consumerreads/returns, last38us max6639us, stable
through~13seconds. No native Bluetooth requests. Todo28 completed immediately;
todo22 resumed. Archive:
`event-buffer-protocol2-baseline-0.6.18-20261019T204925Z/` under backup root.
Deployment manifest updated reboot_pendingfalse and realbufferedvalidationtrue.

User challenged slow progress and libnx support. Acknowledged libnx DOES expose
managed BLE/GATT APIs; mapping30fixtures alreadyworks, major incomplete piece
is getting live Stadia reports intoMC. Generalized guards/diagnostics and our IPC
sizebug consumed time. Agreed focus minimal directknownaddress managed Stadia
verticalslice, not more scan-calibration batches, mixedrawcancel, speculative
settings or repeated diagnostic-tool development. Do not promise workingHOGP
before actual report receipt. User said adv/netloaderon; clarified via structured
question and user explicitly selected **Stadia controller**, not AndroidnRF.

## 26. Managed-only Stadia0.6.19: retry still unconfirmed, verify saved address

Implemented `tools/stadia-probe/source/managed_stadia.inc` included in main after
legacy ConnectionProbe; Makefile dependencies include inc+eventprotocolheader.
--managed-stadia is an explicit dedicated exception; legacy scan/lease modes
remain paused. Host launcher MacBluetoothOFF preflight and reads private host
target INSIDE container, injects --target without printing/persisting address in
Docker command metadata. NRO only accepts exacttarget/mode args. No SD/module/
INI/boot modifications. Source guards permit ONLY standardmanagedSDK APIs:
btdevInitialize/Exit, acquireconnectionevent, ownedconnectionlist, connect,
getcachedGattServices/characteristics, disconnect. No Btdrv/Btm direct commands,
scan, Arm/Stop, retry, pairing API or GATT value writes/subscriptions.

Preflight validates protocols5/1/2, cleanfilter/unusedlease/worker, no long-running
or unstable event phase, validtarget, empty ARUID-qualified ownedconnectionlist.
One connect call; pollownedlist20sec, duplicates/truncated/foreignrecords fail
closed. Only a unique managedtarget triggers cachedservice/characteristiccatalog
queries (5sec cachepoll), revalidatehandle before each; no guessednativeIDs.
Then resolveuniqueownedhandleagain for ONE manageddisconnect. Completion log
requires BTMUchangeevent plus ownedtargetabsence, not raw/native pending retirement.
If request/cleanup unconfirmed: HOLDsdkSession, no rawcancel/retry/btdevExit;
PLUS only logsfullrebootrequirement. Forced appletexit stillmayteardownprocess;
never treat that as cancellation evidence. Fulluserreboot before nextattempt.
AllhostASan/UBSan/ownershiphelpers/source+launcher tests and pinnedARMNRO build/
diffcheck passed. CurrentNRO344120bytes SHA256
`8c97e5be3cb81fdadee1f20f3f74450d32f5ec16fcf157593055a67fadf8865a`.

First actual0.6.19 container
`ddeb2f1016989a9aac44e88f5fd8cb0cb397fd4b3a507467210bfffa995995ff`:
connectIPC0, hook1 BTM1passthrough1, managedtargetneverappeared20sec. BLE13->14,
lasttype4(ClientConnection), allfinished/acked, last135us max6639us. User then
explicitly admitted Stadia NOT inpairingmode. Do NOT count that as a validStadia
compatibility failure. RequestedfullCFWreboot beforeapprovedretry; no newbuild.

User next reported pairing/netloaderon. Freshuptime46sec and alllease/scanstats0
confirmed newbaseline. Retry container
`cbafa88ef092055677e8bb15c30b5b9621713fbe8a505840484dec89c7ddba18`:
- MacBluetoothOFF, managedinit+eventopen0, emptyownedlist.
- ONE connect accepted0; hook1BTM1passthrough1. No scan/arm/worker/rawcancel.
- No managedhandle observed20sec; service/GATTcatalog path NEVER REACHED.
- BLE9->10 dispatches, lasttype4, all10finished/waits/reads/returns; last104us
  max7089us; Core/HIDidle. No observedshared-eventthreadstall inthissample.
- NativeClientConnection STATUS/connectedflag NOT captured by currenttelemetry;
  cannot distinguish terminalfailure vs nativepending/otheroutcomes merely from
  eventtype4/managedabsence. Do not falsely declare connected or fullyretired.
- NRO remains foregroundholdingSDKSession at REBOOT REQUIRED. No PLUSexit,
  nativecancel or repeatedconnect performed. Currentmenuresponsivenessunknown.

Archives underbackupsroot (both include log+anonymousmanifest):
`stadia-managed-controller-not-pairing-0.6.19-20261019T210728Z/`
`stadia-managed-pairing-unconfirmed-0.6.19-20261019T210728Z/`.

Latest operatorrequest: whilecontroller stillpairing, open nRF SCANNER ONLY and
sendStadiaentry screenshot; do NOT pressConnect. Need privatelycomparecurrent
BLEaddress to savedhosttarget before anyretry (neverechoaddress inchat). Current
savedfile staysprivate/unchanged. Switch mayremainholdingprobe whilecapturing;
then fullnormalCFWEMUMMCreboot beforefurtherconnection. Ifaddressmatches, next
usefuldiagnostic isactualnativeClientConnection status/connectedflag, not another
blindretry or scanbatch. CurrentmoduleP2telemetry records eventTYPE/timing only.
PriorunexpectednRFconnection report stilluncorrelated; don'tmistakephoneScannerConnect forSwitchinitiatedlink.

## 27. Address verified privately; native result telemetry ready, FTP unavailable

User supplied readable scanner screenshot via clipboard image. Local Vision OCR
privately compared the Stadia entry with the host saved target: exactmatchtrue,
reversedmatchfalse, confidence1.0. No addresses printed/recorded; no target file
changes. Screenshot shows expected HID1812, LE-only legacy, strongsignal. Phone
connection was not requested. Telegram original path was EPERM; no bypass.

Prepared protocol3/module + NRO0.6.20: unchanged224byte buffered cmd11 ABI size,
uses previouslyreserved fields for global ClientConnection count and last raw
result/status/reason packedscalars with validitybit. Atomic tuple snapshot under
existing sequence; retainslastcallback across unrelatedevents. No address/IDs/
handles/ARUID/payload, no targetownershipinference. No callback delivery/ack/native
behaviorchanges. AllhostASan/UBSan inclcoherentconcurrentcallbacktuple/bounds,
sourceforwardingcontract, bothpinnedARMbuilds anddiffcheckPASS.
Archive `~/.cache/missioncontrol/stadia-backups/native-connection-result-20261019T212304Z/`:
- candidate-exefs.nsp182077bytes SHA256
  `69309748e733ea8a53f94c6ab78f737f754de9e365cdaf359e7a15c45b33c3b5`.
- stadia-ble-probe-0.6.20.nro344120bytes SHA256
  `6af8e0e83cbc1537da28950c7af147e953141f8ffe2932e5a6164d7e6747d54a`.
NOT DEPLOYED. LiveSDstill181763byte531f07...module P2. Expectedlive99file set
comes from event-buffer-fix-20261019T203742Z manifest postdeploy_files.

Askeduserrestartfromheldpendingrequest thenstartFTP. User saidcontinue; one
read-only FTPconnectionattempt timeout8seconds beforelogin, no fileschanged.
No blindretry or network/toolingfallback. Need userconfirmfullCFWEMUMMCrestart
andstartFTP. Then usualmoduleONLYpreflight99files+93IPSsets/backups/staging/
readback/renames/postverify, preserveINI/boot. Activationuserreboot next, then
0.6.20directmanaged-stadia (alreadyincludesbaseline) withcontrollerpairing.
Do notlaunchP3NROagainstP2module: itwillrefuseprotocolmismatch. No further
connectionrequestsincepriorunconfirmedtrial; unresolvedrequestrequiresreset.

## 28. Protocol3 callback-scalar module deployed; activate before next attempt

User enabledFTP. Preflightverified all99scopedfilesagainstP2postdeploymanifest:
oldmodule181763byte531f07...; INI unchanged;93IPS filename sets+hashes and
mitm.lst/toolbox.json/boot2.flag/template exact. Hostbeforebackups complete in
native-connection-result-20261019T212304Z archive. Stagedcandidate182077byte
693097... readbackverified. Renamedlive->
`/atmosphere/contents/010000000000bd00/exefs.nsp.pre-native-result-20261019T212304Z`
andverifiedoldhash, thenstage->live andverifiednewhash. No ambiguousacks/retries.
Separate renames recoverable NOTpower-lossatomic; usertoldnorebootuntilverified.
Reconnected postverifiedall99files,moduleonlychanged,93IPSfilename sets+hashes,
oldSDbackupintact,stagenameabsent. NoINI/IPS/boot/NROwrites, rollback or reboot.
Archive manifest now DEPLOYED AND POSTDEPLOY VERIFIED; USER REBOOT PENDING.

Current LIVE SDmodule182077bytesSHA69309748e733ea8a53f94c6ab78f737f754de9e365cdaf359e7a15c45b33c3b5.
RunningprocessremainsolduntilusernormalCFWEMUMMCrestart. LocalNRO0.6.20
344120bytesSHA6af8e0e83cbc1537da28950c7af147e953141f8ffe2932e5a6164d7e6747d54a.
AskusercloseFTP,rebootnormally,StadiaBluetoothPAIRINGmode,MacBTmustOFF,netloaderY.
Next--managed-stadia includesinitialP3baselinealready; noextra statusuploadneeded.
Neverclaimnewcallbackfieldsproveownership: globalresult/status/reason retainlast
ClientConnection, evenacrossunrelatedevents. No rawcancel/no scans/no explicit
pairingwrites. PendingunconfirmedholdsSDKsessionrequiresreboot. No P3runtime
validation/newconnectionhadoccurredatthatcheckpoint. Savedaddressprivatelyverifiedmatch.

## 29. P3 activated; native Stadia failure captured0x1a/status2/reason0

User confirmedreboot/netloaderready. RequestedactualStadiaPAIRINGthenfreshA;
ran0.6.20 --managed-stadia, MacBluetoothOFFpreflight. Container
`1006eb9d4774a862c2ac795d89285d3b9c85345b4b7f2d1b12f86edb1040a6d9`.
Baselineuptime50sec, cmd11protocol3returnedcorrectly, allscan/lease/worker/hook/
callbackcounts0; BLE9dispatchescompleted/acked, max8428us. One managedconnect
IPCaccepted0. BTMhookattempt1passthrough1, no ownedtargethandlewithin20seconds.
AfterwardClientConnectioncount1, result0x0000001a(decimal26), status2 SDKlabels
Disconnected, reason_field0. BLE10begun/finished/waits/reads/returns, lastwait44us,
alllanesidle. GATTcatalog/reportpathNEVERREACHED. No scans/arm/rawcancel/retry/
pairingrequests/GATTwrites. NROholdssessionREBOOTREQUIRED; cleanup/nativeBTM
pendingretirementNOTestablishedbyglobaldisconnectederroralone.

Communicatedspecificnativeerror and noobservedeventthreadstall. Do NOT claim
exacttargetcallbackcorrelation: telemetrygloballastevent+count, temporalassociation
onlythoughnootherobservedconnect. Do NOT treat0x1aasHorizonResultorHCIunsupported
remotefeaturewithoutprovingnamespace. LocalSDKdeclaresrawresultnonzeroserror but
noerrorenum; webBSAstatusqueriesfailedtoyieldauthoritativemapping. No evidence
obtainedforanexact0x1aname. No flagvariants/rawclientregistration/reset justified.

Archive `~/.cache/missioncontrol/stadia-backups/stadia-native-error-0x1a-0.6.20-20261019T213004Z/`
containslog/anonymousmanifest. Deploymentmanifest updatedactivationtrue,
reboot_pendingfalse,protocol3_runtimevalidatedtrue; installedSHA693097... unchanged.
No furtherhardwareactionafterthisresult. NeednormalCFWEMUMMCresetbeforeanynew
connect, and investigationofnativefailuresemantics/cacherequirements ratherthan
blindretry. VerifyusercurrentmenusresponsivenesswithoutassertingitfromIPCtiming.

## 30. User reset; offline cache/address-type lead, no new hardware test

User had already rebooted and cannotreportpreviousmenuresponsiveness => UNKNOWN.
Laterreportednetloaderready/Stadiapairing, requestedcontinue. No newconnection,
scan,FTP,modulechange or hardwareprobe sent; avoididenticaldirectretry.

DirectpublicGitHubAPIsearch (not genericsearchsummaries) located realvendorheaders:
khadas/buildroot_vendor_broadcom_brcm-bsa branchbuildroot,
3rdparty/embedded/bsa_examples/linux/libbsa/include/{bsa_status.h,bsa_ble_api.h,
bta_gatt_api.h,gatt_api.h}, plusapp_ble/source/app_ble_client.c. Read-onlycopies
under/tmp/missioncontrol-public-bsa-headers. GenericBSAcodesstart100/200; no26map
established. PublicBSAexamplechoosesbondedDBorlastdiscoverybeforeconnect; notproof
ofmandatoryrequirement. HistoricalAOSPandroid-5.1.1_r38stack/btm/btm_ble.c
BTM_ReadDevInfodefaultsBR/EDR+publicandconsultsinquiry/securityrecordsforactual
BLEaddress/devicetype; l2cap/l2c_ble.c directconnectuseslinkrecordpeertype.
PinnedlibnxbtmuBleConnectpassesonaddress+ARUID/PIDwithoutaddresstype. Concrete
cachedependencyhypothesis, NOT exactHOS21.2proof or raw0x1ameaning. Neverinfer
actualaddresstypefromscreenshot/highbitsalone or writenativecache/keys.

Detailednewresearchnote `tools/STADIA-CONNECTION-FINDINGS.md` hassourceURLs/lines,
evidencelimits andrequirementsforasingle-variableSwitchdiscovery-assisted
connectcomparison. PhoneobservationcannotpopulateSwitchcache. Actualscoped
scanmechanismNOTYETimplemented/reviewed; HIDresponseplacementstillblocksoldpath.
No newtool/deploymentneededuntilthatmechanismchosen; no guessedglobalmodewrites,
rawclientregistrationorrawcancelretry. UsercanstopStadiapairingwhileofflinework.

Independentpublicerrorlookupagent2f0ab310-d2b4-4e9 failedproviderSSEparseerror,
returnedNOoutput. No findingsmaybeattributedtoit; userinformedlookupfailed.
Parentcacheanalysiswasindependentandreadactualpublicsource. No backgroundwork
stillrunning. Todo22remainsinprogress; no testsrequiredforresearch-onlynote.

## 31. User asked plan AND start: primary discovery-assisted test implemented

Explained3steps inspect/implement/test+buildbeforehardware. Chosen functional
experiment: existing exactprivateBTMUUID translation nowrecognizesfourthmarker
MarkerPrimaryFlags(firstbyte64). Rewrites ADFlags1 value05 size2(maskff,size1),
observedStadiaprimaryFlags andSDK btdrv_ids.h confirmsenum1. NOTa scan-modewrite.
Default-offexistingexperimentconfig stillrequired. Add/conditiondelete symmetric;
wholeindexdeletion scoped existing tracker. ThreeoldHIDaliases unchanged; fourth
trackedslot ALWAYS excludedfromGetBridgeMarkers andBridgeAdvertisementrejects
flagsbit ifaccidentallypassed. So flagsnativeeventscopies forwardedunchanged,
no extraUUID128 records and no prior formatcalibration required.

New `ble_cached_scan_protocol.hpp`: MCcmd12protocol1 targetrequest8bytes
(address+zeroreserved), inlineoutput40bytes (smallreplyfitsTLS). Modulegetter
`ble::InspectCachedScan` onlyholdsEXISTINGg_event_data_lock, comparesnativecache
address, returnsmatch+freshsequence+scanresult/status+MATCHEDpeerdevice/address
metadataandadcount. NOsignal/ack/consume/cachewrite,newpeeraddress/payloadstorage,
rawdumporlogging. g_event_sequenceincrementunderexistingnative-readlock. Invalid
requests/defaultoffdenied. Unknown/unmatchedpeer metadatazero. Thisiscachedcurrent
eventONLY, sooverwritescanmissadiscovery; negativeisinconclusive,neverassumedseen.

NRO0.6.21 `--discover-stadia` is explicitdedicatedexceptiontolegacyscanpause:
- sameprivatetargetinjection+MacBT_OFFlauncher, freshphysicalA,cleanprotocols5/1/3,
  zero priorconnecthook/callback activity, emptyARUIDownedlist;
- reads cmd12capability+sequencebeforeanyscan, max30secSmartDeviceFLAGSmarker,
  pollsnon-consumingcache andONLYexacttargetfreshvalidBLE/addressmetadata qualifies;
- ALWAYSstoponceafterstartincludingstarterror; verifyexactprivateadd/remove1/1,
  active0failed0, scan_errorsunchanged, allbridgecountersunchanged. No filterclear;
- cleanmissingtarget=>NOCONNECT andnormalSDKexit; uncertaincleanup=>HOLDsession
  rebootrequired. PLUScancelsobservationbuthasnoeffectonrequiredscanstop;
- freshmatch+cleanstopped+postscanpipeline/ownershipchecks=>SAMEoneSDKmanagedconnect,
  cachedHIDcatalogifownedhandlethenmanageddisconnectasbefore. No metadata injection,
  no rawcancel,retry,ownGattclient,explicitpairingorvaluewrites/subscriptions.

Files: modifiedscan_filter/tracker/advertisement,bluetooth_ble.{hpp,cpp},btdrv_mitm
SDKstaticassert,mc_service.{hpp,cpp}; newble_cached_scan_protocol.hpp;
newtools/stadia-probe/source/{managed_discovery.inc,discovery_evidence.hpp},
modifiedmanaged_stadia.inc/main.cpp/Makefile,launcher/testguards/tests.
SDK-freehelpertestsaddedfilterdefaultoff/foreignexactness/symmetricdelete,flags
slottrackingNOalias,cachewrongtarget/stale/errors/truncation/metadata privacy,
cleanuppredicateuntrustedstop/statusread/errors/counterwrap/badprotocol/busy,
connectgatecancellation/missingfreshness. AllASan/UBSan suites+fakeIPC/source/native
SignalWaitforwardingcontracts, bothpinnedARMbuilds anddiffcheckPASSafterfinaledits.
Independentreviewagent3b8acaaa-2a3e-463 failedmodel_not_supportedbeforeanytoolcall;
NOreviewresult. Reportedthatfailure,usernotmisled. Parentlocalreviewcompleted;
doNOTcallcandidateindependentlyreviewedorhardwarevalidated.

Candidate archive:
`~/.cache/missioncontrol/stadia-backups/primary-discovery-20261019T215241Z/`
- candidate-exefs.nsp182900bytesSHA256
  `2d6fdd54f841a5b8bc8253cbe17c9b56e47724ac94b6abe75284a087b9d4f875`.
- stadia-ble-probe-0.6.21.nro348216bytesSHA256
  `f28db0f5a802e00e3143166bde565f1c0cc5934629a23c6eb5d9ddafd4cf4b67`.
- prior_manifestnative-connection-result-20261019T212304Z/postdeploy_files is
  expectedlive99files; currentmodule182077byte693097... notnewcandidate.

**Await userFTPready.** Tolduserfunctionalchangeupfront, nohardwaretest/files
changedyet. Nextsameprovenmodule-onlypreflight99files+93IPSfilename/hashsets,
backups/stage/readback/oldlivebackup/newlive/postverify. NoINI/IPS/boot/NROwrites.
ThenusernormalCFWEMUMMCreboot andStadiaPAIRING+netloaderY; launch--discover-stadia
ONCE with0.6.21 (initialcmd12capabilitychecksavoidseparatebaselineupload).
Needoperatorcurrentmenuresponsivenessifpossible; priorreportedunknownafterreset.
DoNOTreplayrawcancelorskipfreshmatch/cleanstopsjusttoprogress. Researchnoteupdated
`tools/STADIA-CONNECTION-FINDINGS.md` implementationvsHOSproofclearlyseparated.

## 32. Primary-discovery module-only deployment and failed-start hardware trial

UserenabledFTP. All99livefilesmatchedpriorP3postdeploymanifestincludingmodule
182077bytesSHA693097...; INI213bytesSHAac49acdf...13da. Protected93IPSfilename
sets+hashes andmitm.lst/toolbox.json/boot2.flag/template verified; hostbefore/
backupscompleteunderprimary-discovery-20261019T215241Z.
Stagednew182900bytecandidate2d6fdd54... readbackverified; live->SDbackup
`/atmosphere/contents/010000000000bd00/exefs.nsp.pre-primary-discovery-20261019T215241Z`,
oldhashverifiedthenstage->live,newhashverified. Separate renames NOTpower-loss
atomic; userwarnednotrebootmidtransfer. No ambiguousacks/retries/deletes.
Reconnectedpostverifiedall99files(moduleonlydifferent),93IPSfilename sets+hashes,
originalbackupandstagenameabsent. NoINI/IPS/boot/NROwrites,rollback,assistantreboot.
Manifest was DEPLOYED AND POSTDEPLOY VERIFIED; USER REBOOT PENDING.

LIVE SDmodule182900bytesSHA256
`2d6fdd54f841a5b8bc8253cbe17c9b56e47724ac94b6abe75284a087b9d4f875`.
Local0.6.21NRO348216bytesSHAf28db0f5a802e00e3143166bde565f1c0cc5934629a23c6eb5d9ddafd4cf4b67.
Runtimeolduntiluserreboot. User confirmed pairing+netloader. Launched archived NRO
ONCE via --discover-stadia; MacBT_OFF. Protocol5/scan1,connection1; no prior
activity, owned handles absent, Core/HID idle. Managed init/event acquire and cmd12
inspect passed. btdevStartBleScanSmartDevice returned raw IPC0x00052a8f; exactly
one btdevStopBleScanSmartDevice followed, success. polls0, target_seen0, trusted0,
cleanup0. No managed connect, retry, cancellation or GATT request. Probe log said
'exiting'; its purported hold loop did not keep applet resident. Treat SDK stop
success as insufficient cleanup proof. Full normal CFWEMUMMC reboot REQUIRED before
further Bluetooth activity. Menu responsiveness after run unknown. Raw start result
namespace unresolved; do not label HCI. Archive log
`primary-discovery-20261019T215241Z/run-0.6.21.log`, manifest says
`DISCOVERY START FAILED; NO CONNECT REQUEST; USER REBOOT REQUIRED`. Do NOT retry
before analyzing raw add/start failure and tracker counters; no raw filter clear.

## 33. Investigation: AD type is NOT a supported native condition; corrected v22 candidate

User: investigate and say when to enter pairing again. ToldleaveStadiaoutofpairing,
no console retry. Read pinnedbtdev/btmu wrappers, modulefilterhooks,tracker,
fixture,smallcached/probe files; fetchedSwitchbrewdriver+BTM+Error_codes. The
`BleAdvertiseFilter.CondType` table lists2/3(UUID16),4/5(UUID32),6/7(UUID128),255
manufacturerONLY. Mask/size specificallymanufacturerused. ADFlags1enumdoesNOT
certifyfiltercondition1. Ourv21Flagsrewritewasunsupportedassumption. Publicdocs
notexactHOS21source, so missingtype1 is evidenceagainstassumptionnotfullbinary
proof. Scanstart0x52a8fmechanicallymodule143description661, NOTmappedbypublictable;
no verifiedHCIorBtm symbolicmeaning. Couldnotattributeexactfailingoperation
becausev21ReadPatchStatusonlyprintedhandshake, not counters. Stop successalone
notcleanup. UserwasexplicitlytoldmyFlagsmistakeandprobablecause; no blamingStadia.

Better-supportedpathEXISTSwithoutglobalchanges: priorcompleteHID16nativefilter
acceptedandreported3nativeHIDadswithbalanced1/1cleanup, while BTMmanageddelivery
negative. Newnativecacheexacttargetcheckremovesthatmanageddeliverydependency.
Historicalnativeadsnotexact-target-correlated; nextseenStadiastillunproven.
So replacednewfourthslotwith`NativeHid16`exactmarkerfirstbyte65, sameCondType3,
size3,masksize2,UUIDbytesasoldBigEndianfilter. TestassertentirestructBYTEIDENTICAL
at sameindex. Native-onlyslotNOTinGetBridgeMarkers; NOalias,eventrewrite/global
modewrite/cacheinjection/newGattclient/rawcancel. Retiredmarker64PrimaryFlags
recognizedONLYforrewritefalsebeforemutation;testsassertNEVERtype1. Existing
threealiasmarkersunchanged. CachedMCcmd12P2certifiesnewmarkerimplementation;
v22checksP2BEFOREscan,P1installedmodulefailclosed;oldv21refusesnewP2.

Probe0.6.22uses30secboundednativeHIDscanwithsameexactfreshsavedtarget+valid
metadata->oneStop->one-add/remove/noerrors/bridgeunchanged->pipelineidle+
ownershipgate->sameonesinglemanagedconnect. No target=>noconnect, unconfirmed
cleanup=>reboot. Nowlogsbaseline+post-stopledgerandnativeerrorsonstartfailure.
DoNOTclaimappletcanholdSDKsessionafterforcedexit; normalrebootstillrequiredif
cleanup/pendingunknown. Previousrebootpostfailedtrialnotconfirmedyet.

AllASan/UBSanhostsuite/sourcecontracts/fakelauncher/cleanupcachetestsPASS;
newretiredFlagsno-mutation/newUUID16bytesmatching/native-onlyno-alias/
oldP1evidence-rejection/before-scancapabilitysourceguardsPASS.
BothpinnedARMbuilds+diffcheckPASS. No separateagentreviewthisturn; parentreview.
NoFTP/netload/consoleactivityduringthisinvestigation.

Archive`~/.cache/missioncontrol/stadia-backups/native-hid-discovery-20261020T053803Z/`:
- candidate-exefs.nsp182784bytesSHA256
  `60293ec940020e70f0c81724569d8552cc36930113d66edef21d50efb20008e9`.
- stadia-ble-probe-0.6.22.nro348216bytesSHA256
  `1a65447f88498467a3afb62408052d7f8200fc1937ca76c91ae8a3f138d5f8c9`.
- prior_manifestpointsprimary-discovery-20261019T215241Zpostdeploy_files99files;
  currentliveexpected2d6fdd54...182900oldfunctionalmodule, NOTP3orig693097....

**Needs FTP foranotherfunctionalmodule-onlyupdate**, same99filepre/postbackup
and93IPSfilename/hash/INIbootprotections. No new live operationauthorizedjustby
thisinvestigateask. TelloperatorFlagsapproachretired,correctedbuildready,
FTPfirst; keepStadiaoutofpairingduringtransfer. AFTERverifiedupdate andnormal
CFWEMUMMCreboot, then explicitlyaskpairing+netloaderY. FirstnexttrialONLYv22
--discover-stadiawithMacBToff. Don'trunv21oridenticalfailedFlagsagain.
ResearchfindingsandprobeREADMEupdatedwitherrorinferencevsknownfacts.

## 34. Corrected native-only HID module deployed; now request reboot then pairing

UserenabledFTP, additionallyreportsStadia brieflyblinkedwhite(~1sec)duringprior
v21probe. Loggedmanifestoperator_observation; no instrumentcorrelation. Prior
application sentNOmanagedconnect request andbaseline/posteventhookcounts0;
LEDdoesNOTproveconnectionoridentifyactor. Don'tinfernootherclient/nativeactivity.

Preflightall99expectedfilesmatchedprimary-discoverymanifestpostdeploy_files,
including182900byte2d6fdd54...oldmodule. All93IPSfilename/hashsets+INI213byte
ac49acdf...13da+protectedbootmetadataunchanged. Savedall99hostbefore/backups.
Staged182784byte60293ec9...moduleandverifiedreadback. Oldlive->SDbackup
`/atmosphere/contents/010000000000bd00/exefs.nsp.pre-native-hid-discovery-20261020T053803Z`
verifiedoldhash;stage->liveverifiednewhash. Reconnectedpostverifiedall99files
onlymodulechanged,93IPSfilenamehashsets,INI,mitm.lst,toolbox.json,boot2.flag,
templateunchanged;oldSDbackuphashverifiedandstagenameabsent. Noretries/errors,
noINI/IPS/boot/NROwrites,remote resource deletion,rollback,assistantreboot.

LiveSDmodule182784bytesSHA256
`60293ec940020e70f0c81724569d8552cc36930113d66edef21d50efb20008e9`.
Archive`~/.cache/missioncontrol/stadia-backups/native-hid-discovery-20261020T053803Z/`
manifeststate`CORRECTED MODULE DEPLOYED AND POSTDEPLOY VERIFIED; USER REBOOT PENDING`.
LocalNRO0.6.22 348216SHA1a65447f88498467a3afb62408052d7f8200fc1937ca76c91ae8a3f138d5f8c9.
ModuleonSDnotactiveuntilusernormalCFWEMUMMCreboot. TellusercloseFTP,reboot,THEN
StadiainBluetoothpairing+netloaderY. Nextone--discover-stadiawith0.6.22andMacBT_OFF
preflight;MCcmd12P2beforeanyscan. SupportednativeUUID16notaliases; exactfreshsaved
target+clean1/1stop+idleownershipgatesameoneconnect. Targetnotseen=>no connect;
uncleancleanup=>reboot,no rawcancellation/retries.

## 35. v22 sent, capability refused before scan; running module mismatch

Usernetloaderon; explicitlyaskedcurrentpairingandconfirmedorangeflashing.
VerifiedlocalNRO348216SHAmatchesarchivedv22candidate, MacBT_OFF launcher, sentonce.
Modulebasicstatus5scan1 and connection1 allzero counters; Core/HID/BLEbalancedidle,
BLE17events,last7,uptime1216411ms(~20min16sec). ManagedbtdevInitialize and connection
eventacquirepassed. cmd12InspectcachedscanIPCsuccess, then immediate
`No fresh trusted target observation; NO connect request. Scan cleanup confirmed.`
No secondReadPatchStatushandshakeandnoAcquirescopesscanevent. In exactbuilt
managed_discovery.inc,shortcircuitis`before.protocol_version != cached::ProtocolVersion`
BEFORE secondReadPatchStatus; hence runningcachedscanprotocol!=2. Actualreturned
numbernotlogged, doNOTclaimobserved1. No scan/start/stop/connect/cancel/GATT requests.
This is compatibility refusal, NOT target-discoverynegative nor new0x52a8f.
Misleading genericendmessage sayscleanupconfirmed butscanNEVERSTARTED.

AdvisednormalFULLCFWEMUMMCreboot(notHBLrestart) toactivateverifiedonSDmodule, then
orangeflashingpair+netloaderagain. DoNOTFTPdeployagainblindly. Ifsameprotocolmismatch
AFTERconfirmedfullreboot, firstinspectcorrectmodulebuild/dependencies/SDenvironment
before hardware retry (protocolvalue needslogging; don'tblameoperator). No testsor
sourceeditsmadethisrun. SDKsessionexitednormally;no uncertainpendingrequest.
Log`native-hid-discovery-20261020T053803Z/run-0.6.22-protocol-refusal.log`; manifest
`V22 REFUSED BEFORE SCAN: RUNNING CACHE PROTOCOL MISMATCH; FULL USER REBOOT NEEDED`.
Fullnew discovery trial stillnotreached. CurrentSD182784SHA60293ec9...unchanged.

## 36. Second capability refusal; stop retrying; local v23 read-only probe ready

UserreadythenfreshA; sentexactarchivedv22onceMacBT_OFF. Samepreflightreturnafter
cmd12IPCsuccess,beforesecondscanstatushandshake; no scan/eventacquire/start/stop/
connect/cancel/GATT. Uptime1216411->1440044ms,Corecounts13same+BLEmax9605ussame;
consistentwithsameprocessuptimebutNOT enough to identifyincompatiblefileorclaim
userdidn'treboot. Userexplicitlyinformedprior'oldmodulestillrunning'explanationtoo
definite, don'taskidenticalreboot/retry. ToldleaveStadiaoutofpairing. Appletexited
normally,no newpendingrequest/cleanup. Logrun-0.6.22-second-protocol-refusal.log
archivedsamefolder,manifeststateSECOND V22 CAPABILITY REFUSAL; PAUSE RETRIES.

Offlinecheckedgetter, MCcmd12table,libnxdispatchmacros andSFOutserialization;
no specificABIdefectproven. DisassembledACTUALmoduleELF: InspectCachedScan
non-scanbranchmovw2,#2storeoffset0;scanbranchmovx0,#2...writesprotocol2too.
LocalNSPhashsizeexactmatchesdeployedarchive60293...182784. Thisproveslocalbuild
isP2,NOTwhichreplyactuallyarrivedfromconsole. Needactualprotocol+buildstrings.

BuiltNRO0.6.23withREADONLYexisting--event-progressmodeaugmented:
- existingstatus-onlyProbe before anyBTinit;
- one MCcmd12InOut withsynthetic6bytecache-comparison-token(onlyinMCgetter,NEVER
  passedtoBluetoothservice);outputONLYprotocol(expected2),eventtype,sequence;
  NOpeer-match/device/addressmetadataorpayloadprinted,NOrealtargetneeded;
- MCcmd1buildversion64char+cmd2date32charboundedformatting;
- same12event-progresssnapshots; NO BTinitialization/scan/connect/pair/cancel.
Discoveryv23nowlogsactualprotocolandhasUnavailableoutcomewith'NO SCAN STARTED;
No scan-cleanup claim',ratherthanmisleadingmissingtarget/cleanupconfirmed.
DoNOTuse--discover-stadiatoretrieveversions;nextaskpure--event-progressONLY.

FullASan/UBSan/source/fakelauncher suites+NROpinnedARMbuild+diffcheckPASS.
OneinitialsourceguardfailedNameErrorbecausemanagedusedbeforedefined; fixedguard
placementandreranALLPASS. Nosuppressedfailures. No moduleedit/rebuild/deployment,
no newconsoleopsaftersecondrefusal. NewNRO348216bytesSHA256
`bb5eba22186d6c02b73a79ce4b6641d6578a447a5c30900bed3161d283808e4b`,
archivedasstadia-ble-probe-0.6.23.nroinsamefolderandmanifestcandidateentry.

Nextrequestoperatorpermission/read-onlynetloaderreadywithStadiaNOTpairing;
NOFTPneeded. Then netload--event-progress withfreshA. Actualreturnedprotocol+
publicbuildstringswilldiscriminateoldmodulevsreply/layoutvsunknown. Donot
inventnativeerror/targetabsencebeforethat. Allradio-changingmodespauseduntil
capabilityclear;no newbackgroundjobactive(exceptolderfinishednxlinkcontainers).

## 37. Read-only capability1 observed; full restart confirmed; actual package is P2

Usernetloaderon+pairing; toldpairingnotneededmaystop. Sentexactv23--event-progress
ONCE, no Macradio preflightneededbecauseMC-only. FreshA;12samplescompleted.
MCcmd12 actualreturned_protocol1 expected2,eventtype7,sequence29 (matchesBLEtotal29).
MCbuild0.16.0-master-71bbe2d,builddateMonOct19 06:44:51PMUTC2026; dateobjectmay
be cachedacrossincrementalbuilds soNOTunique moduleidentifier. Uptime1917411..
1930891ms,core13idle,HID0idle,BLE29balancedidle,lastwait179us/max9605us.
NoClientConnectionevents/connecthookattempts. NoBluetoothinit/scan/connect/cancel.
Thisisoldcapabilityindeed, nottargetnegativeorGarbageversion0; whyactivationfailed
stillunresolved. Logrun-0.6.23-read-only-capability.logandmanifestarchivedsamefolder.

Askedstructuredrestartmethod: userexplicitlyPOWERmenuPowerOptionsRestart.
Acknowledgedfullrestart,donotrequestrepeatidenticalaction. ToldcheckingPACKAGED
modulebecauseELFaloneinsufficient. ParsedactualNSP PFS0 entries:main.npdm964bytes,
mainNSO181724bytesSHA256bdcc89c783c11ec517f7c61d8de5006bceca6f79d467847e5fdd2d4fe6c05743.
NSPmainbyteidenticalcurrentNSO;boundedstdlibLZ4decodedtextverifiedNSOheaderhash;
NSO.textbyteidenticalELF.text. AtARMaddress0x1d7c opcode0x52800042 movw2,#2,
scanbranchlikewisereturns2. NSPSHA60293...exactdeployed182784. ThuspackagestaleELF
hypothesisNOTsupported. No codeedits/builds/consoleoperationsduringthischeck.
ExternalAtmosphere loaderdocs confirmsfull exefs.nsp overrideandloose exefs/individual
fileoverrides exist; NOT yet determinedprecedenceorwhetheractualSDhasloosefiles.
https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/loader.md

**NextaskFTPforREADONLYpost-restartverification**, notdeployment: hashactualSD
exefs.nsp,inventorymodulefolderandpossiblelooseexefs/mainoverride,readbootselection
metadataonlyifneeded. Protectedfilescurrentexpectedarchivepostdeploy_files99. No
newwrites/reinstalls/deletes/scan/pairingrequestsjustbecauseprotocol1. Needoperator
FTPavailability/permission; no liveFTPattemptyet. Don'tdumpbootconfigsecrets or
addresses. OriginalprivateLEDblinknotconnectionevidence. Activationproblemstill
blocksfunctionalStadiatrial; noconnectionsattemptedsincev20.

## 38. Read-only FTP inventory clean; v24 reads loaded module NSO build ID

UserFTPon authorizedREADONLYaudit. UsedonlyCWD/NLST/RETR (plusFTPconnection/login),
noSTOR/rename/delete/radio. Saved99filespostrestartunder
`native-hid-discovery-20261020T053803Z/activation-readonly-20261020T060828Z/`.
All99hashesmatchpostdeployexpected;current182784byteexefs.nspSHA60293...indeedP2.
ModulefolderNOlooseexefs/directory;onlyexefs.nsp,flagset/mitm.lst/toolbox,
oldnonmatching-extensionbackups andAppleDoublemetadata. DoNOTdeletebackupsor._.
FirstIPScomparisonincorrectlycounted._*.ips(65/61/56), thenfixedtooriginalpolicy
excludeleadingdot; allnormalsets33Bluetooth+31BTM+29HID=93matchbaseline;ALLoriginal
hashesunchanged. Auditreportcorrected,notarealIPSincident. No bootwrites.
Bootloader/hekate_ipl.ini509bytehashb84f45...175d,autoboot4/list0;config4pkg3
standardatmosphere/package3+emummcforce1;emuMMC108byteinihash6f34...221enabled1
RAW1path. Nooverride_config.ini/stratosphere.ini. Noexefsoverridekeys or extraKIP
entries inbootconfig; bootloader/iniempty. Didnotdumprawconfigs/addresses/secrets.
No explanationyetforP1runtimeagainstP2onSD;userPower-menuRestartconfirmed.

TolduserFTPchecksallpassandcheckingloadedbuildidentityratherthananotherrestart.
PinnedSDKprovidesread-onlypmdmntGetProcessId(fixedMissionControl010000000000bd00)
andldrDmntGetProcessModuleInfo. ModuleBuildIdis32bytes,pids/baseaddressesnotneeded.
ExtractedarchiveNSOheaderbuildIDs,uniqueandcodeidentitynotcachedcompiledate:
- P2correctednativeHID:03ff63e3187d2cf0dec6d80b68fe2c85f44b4a73 +12zero bytes
- P1retiredFlags:200543ad644a029a43f9574053c8b5600a1d448a +12zeros
- pre-discoveryP3diagnostic:2748fc6ad121643ee56db567fac4c3a3fc9ec42b +12zeros.

Builtv24NROONLY(no sysmoduleedit/deploy):newmodule_build_id.hpppureclassifier+
module_identity.incmetadataquery calledinsideexisting--event-progressAFTERstatus-
onlyMCinitialization. PMlookup/exit thenLDRmetadata8entrybound/exit. Reportpublic
NSObuildIDsandknownarchiveclassificationONLY;neverPID,base,size,memory/code,
debugattach,processcontrol,programarguments,arbitrarytargetorBluetoothservice.
Permissionerrorsstopmetadataquery, no privilegeworkaround/no radiofallback.
If8entriescapacityreachedexplicitlydeclarescompletenessunknown. No realBTtarget
needed, previouscachecapabilityreadusessyntheticMCcomparison-tokenonlyasbefore.

Tests:fullASan/UBSan/fakelauncher+sourceguardsPASS;purebuildIDtests3known,null,
mutateeach32bytes(includingtrailingzeros)=>unknown. Whitelistonly6PM/LDRSDK
initialize/exit+getPID+getModuleInfo functions;sourceguardnoBT/svc/base/memorydump.
PinnedNROARMbuild+diffcheckPASS. Candidatearchivedsamefolder:
`stadia-ble-probe-0.6.24.nro`,356408bytesSHA256
`508148b23224e4fa4c0fdef63156b98a4bcec472b8c29c3f4e65383179f0b264`.
No newNROuploadafterv23; no backgroundwork. **Nextasknetloaderreadyforread-only
--event-progressv24**, NOFTPwrites/reboot/pairing. UserwasstillinFTP;closeitfirst.
LoadedIDwilldiscriminateoldcodevsP2andfurtherreply/serviceinconsistency. Don't
claimloadingwrongfile/rootcauseuntilthatread. No reasonforrawcancel/globalreset.

## 39. v24 identifies old code; Hekate reboot then proves corrected P2 running

Usernetloaderon+nopairing. Sentexactv24 --event-progress, freshA. PM+loader
metadataAPIsALLsucceeded,returned1module,buildID
`200543ad644a029a43f9574053c8b5600a1d448a000000000000000000000000`
EXACTmatchretiredFlagsarchive. CacheP1,type7seq37matchesBLE37. All12snapshotsidle,
Core13,HID0,BLE37balanced,connections/hooks/worker/cancel0. Thisprovesoldloaded
code(notABIreplybug)despitecorrectP2NSPonSD. Logrun-0.6.24-loaded-module-identity.log.
Tolduserresult,PLUSsafeexit,andbeforefullpoweroffaskedusualCFWbootmethodready.
Usercustomreply: **rebooted from hekate; netloader up**. No need moreboot questions.

ReranSAMEread-onlyv24 afterthisexplicitHekateboot(no pairing),freshA. PM+loader
metadatareturned1module,loadedID
`03ff63e3187d2cf0dec6d80b68fe2c85f44b4a73000000000000000000000000`
EXACTcorrectednativeHIDP2archive. Cacheprotocol2expected2,type7seq9 coherentwithBLE9.
Uptime58033ms; Core/HID0idle,BLE9balancedidle(last55us,max6235us),ALL12samples
unchangedidle. No priorconnecthooks/nativeClientConnection/filteractivity/worker/
cancel. SDKcacheandmetadatahelpersvalidatednoaddress/payloadlogging. NoBTservice
init/scan/connect/cancel/processcontrol/FTPwrites. ProbeawaitsPLUSsafeexit.

**Activationnowresolved.** Don'tspendmoretimeonwhypriorPowerRestartdidn'tactivate;
wehavepropernewboot+loadedcodeandcanresumeactualverticalslice. Don'tblameoperator.
Log`run-0.6.24-after-hekate-correct-module.log`;manifeststateCORRECTED MODULE ACTIVATED
AND LOADED BUILD ID VERIFIED AFTER HEKATE; AWAIT STADIA PAIRING FOR FIRST NATIVE HID TRIAL.
reboot_pendingfalse,runningprotocol2,correctbuildID recorded. Generalhardware_validation
false stillmeansnativeHIDfilter/exacttarget+connectionexperimentnotrunyet.

Userexplicitlytoldcorrectmoduleactive,PLUSexit,Stadiaorange-flashingpairing,
netloaderYthenready. **Nextactualtrialv24--discover-stadiaONCE** withMacBT_OFF
launcher, no more read-only baselineuploads/FTPorrebootsneeded. LocalNRO356408
SHA508148b23224e4fa4c0fdef63156b98a4bcec472b8c29c3f4e65383179f0b264.
Newv24ReadRunningIdentityonlyin--event-progress,notinmanagedtrial. Discoverylogs
cmd12P2actualbeforeanyscan;native-onlysupportedcompleteHID16marker65, NOaliases.
FreshEXACTsavedtargetnativecacheevidence+metadata andscopedstop1/1noerrors/
bridgeunchanged+idle/ownedlistgatesSAMEoneSDKmanagedconnect. NoTarget=>noconnect;
uncertaincleanup/pending=>reboot,no rawcancellation/retry/unowneddisconnect.
FirstactualnativeHIDscanwithP2stillNOTRUN. No currentjobbeyondappletawaitPLUS;
allhostlogfollowcallsendedtimeoutaftersnapshotsfinished,notongoingbackgroundtests.

## 40. First corrected supported native-only HID scan: clean negative, no connection

User'done'inresponsetoorangeStadiapair+netloaderready. Verifiedlocalv24NROexact
hash356408SHA508148b2...,previouscorrectrunningmoduleP2known;guardedlauncher
MacBluetoothconfirmedOFF,netloaded--discover-stadiaONCE,freshA.
BaselineP5scan1,connection1zero/counters,alllanesidle;nativecacheP2expected2,
seq17type7. Native-onlymarker65completeUUID16condition3(noaliases)startIPCsuccess;
575cachedpolls~30sec, exacttargetNOTseen. StoponceIPCsuccess. Poststop:
added1removed1active0failed0last0,driver_ads0driver_hid_ads0,
managed_events2scan_events0scan_errors0lasttype7,bridgecopies0/no_space0/
unvalidated0/rejected0. Hencecleanup1trusted1. Notezero nativeadvertisements,
notjustmanagedscanlistfailureorassumingmissedcurrentcacheoverwrites. Lastnative
scanstatuseventarrivedafterfilterremovedsofilteredcountersscan0notfulleventcount.
NativeBLEdispatch17->23balanced,finalidle,lasttype6,lastwait32us,max6235us.
Core9/HID0idlebalanced. NativeClientConnection0/connecthooks0/worker0/cancels0.
NOmanagedconnectrequest,leasearm,rawcancel,GATTcall,scopealterationorretry.

Tolduserstart/stopworksandcleansafelybutnativeads0=>noconnect. PLUSsafeexitand
stoppairing;no rebootneededforthisrun'scleanup. Don'tblindlyrepeatcompleteHIDscan.
CurrentHOME/sidebarresponsivenessnotreported;idleeventlanesaloneNOTUIproof.
AppstillwaitsPLUSwhenlastlogread;nxlinklogfollowtimeoutstoppedhostview,notnewjob.

Archive`native-hid-discovery-20261020T053803Z/run-0.6.24-native-hid-discovery-negative.log`,
manifest`FIRST CORRECTED NATIVE HID DISCOVERY COMPLETED CLEANLY; ZERO ADVERTISEMENTS;
NO CONNECT`,reboot_pendingfalse. Runtimefiltervalidationstart_stop_acceptedtrue,
balanced_scoped_cleanuptrue,exacttarget_discoveryfalse,live_reportsfalse.

**Nextworknotyetstarted:** decidefromactualnativefilter/scanresponseimplementation
whethercondition2incompleteHIDratherthan3completecanmateriallydiffer (bothpublic
mappingServiceUuid16,don'tassumedifference), andwhetherthenativepathscansresponse.
Prior3HIDadobservationwasnotexacttargetcorrelated; mayhavebeenphonecontrol,not
proofStadianativeseen. DoNOTclaimStadiaaddresswrong/notpairing/cachemetadata
hypothesisdisproved—discoveryprerequisitewasneverachieved. Flagsfilterretired,
no inventedaddress/name/manufacturer/rawHCIglobalclear/modewriteworkaround.
No nexttest/deploymentauthorizedjustbybeingnetloader/FTPavailable; explainconcrete
newvariablebeforeanotherhardwaretrial. Goalremainsone managedlink+reports,notmore
genericcontrolbatches. Installedmodule60293...182784P2verifiedrunning,93IPS+INIboot
preserved,localNROv24still508148b2.... No newfiles/codechangesforthisrun.

## 41. User continue: source investigation rules out speculative UUID-list flip

No new console request, FTP operation, scan, deployment or source-code change.
Parent read pinned btdev/btmu/btdrv wrappers, current observers and existing
bluetooth_code_export.hpp/broker. btmuSmartDevice sendsUUID+ARUID+PID; public
btdrvSetBleScanParameter accepts interval/window ONLY. No scoped active/passive
selector documented. Context7 tools unavailable; used pinnedSDK and primaryweb
sources instead. Two bounded read-only agents completed:
- 130e0aa0-0079-4b3 localprovenance: earlier3driverHIDads+3aliases were Android
  --inspect-control context; actuallog65-87 says syntheticadvertiser and'Stadia
  is not needed'. No retainedidentities or contemporaneousorangepairing. Parent
  verifiedactualexcerpt. This proves filter/controlpositive, NOT Stadiasighting.
- 7f1c097e-5b2d-47f publicsemantics: Switchbrewgroups2/3ServiceUuid16 byteswapped;
  BSAone srvc_uuid condition; historicalBluedroidparserpartial/complete branch
  shared. No distinct3->2remedy established, Switch21.2 firmware stillunknown.
  AndroidAPCFofficialcontractexplicitadvertisementANDrelatedscanresponses through
  filters. HistoricalBluedroid appendsresponseAD andsometimeswaitsresponsebefore
  reportingactiveinquiry. BSAis_scan_rsp belongs OUTGOINGADV config, NOT scanner.
  Parent verifiedofficialAPCFpassageandBSAunion/fieldcontext via fetchedsource.
  Neither privatecontrollerfilterimplementation norSwitch21.2 active/passive/
  matchingmode found. No safe scoped discoveryfix proven. Bothagentsnoedits/HW.

Sources/passagesandboundariesappendedtools/STADIA-CONNECTION-FINDINGS.md.
No justification for another complete/incomplete/nativeUUIDwidthcontrolbatch,
Flags/name/appearanceinventedconditions, globalclear or guessedmode flag writes.
Stadia's combined35bytephoneviewstilldoesNOTindependentlyprovepacketboundary.
Directconnectcachehypothesis remainsunreached untilSwitch-observedtarget achieved.
Do NOT promote Android3ads into Stadiaevidence or claimdirect0x1a HCI mapping.

Next concrete evidence source: exactnativeHOS21.2 btdrv(010...000b) andbtm
(010...002a) /mainNSOs. Existingprobe --console +tools/stadia_sd.py code route
implementsfixedtwoaliases /btdrv,/btm <=16MiB normalfilesystemcodeaccess,
SHA256+NSOcheck andhost-onlyexclusivecreate;no caller-definedprogram/path,
keys,rawNANDorSDwrites. It is NOT a running-memory dump. No successful prior
nativecodeexportfoundinlocalartifacts;existingimplementationisnotproofaccessworks.
If denied,reportexactResultandSTOP,notprivilegeworkaround/rawstoragefallback.
Explainthisnewread-onlysessionandobtainoperatornetloaderreadybeforelaunch.
No pairing/FTP/moduleupdate needed. Currentv24alreadyincludesexportimplementation,
fullhosttests/sourceguardspreviouslypass. Brokerisforegroundoperator-authorized
session; don't imply autonomous/backgrounddeviceoperations. Ensureonlycode
operations+quit, no SDput/radiocommands. Actualbinaryanalysisnotbegununtilexport.
Useralreadytoldfindingsandproposedexactnativecodeevidencesource; finalaskready.

## 42. Both code exports failed; green flash was return to hbmenu, not observed fatal

Userreadyforboundedread-onlyexport. Ranhostcoordinatorstandardforegroundconsole
withsamev24NRO356408SHA508148b2...,freshA. ExistingSDbrokerfilesessionONLYcode
operation /btdrv tohostexclusiveNSOdestination; no SDput/radiocommand. Default
fsp-ldrroute returnedCodeUnavailable8 with rawIPC0x00000615. STOPPEDatfirsterror,
btmNOTattempted,nofallback. Queuedquitonce,confirmedNROREMOTEIDLEcleanup1/Finished/
peerdisconnected,hostexit0. Archive native-code-export-20261020T064415Z report+log.
NoNSOfilecreated. Pinned sm_results.hppmodule21desc3 mapsOutOfSessions,NOTBLEerror
orpermissiondenial. Exactfailingloaderstageunlogged;sessionacquisitionplausible,
don'tclaimdifferentserviceownerorfreeunownedresources.

Userexplicitnetloaderlisteningagain,retrywhenneeded. Explainednotrepeatloader;
verifiedpublicFilesystem_services FileSystemProxyTypeCode0 andOpenFSWithIdAPI,
pinnedfs.cattr/versionwrapper+existingnormalFSfixedNCMtitlelookup route. This
establishesdocumentedtype/requestshape,NOTguaranteearbitrarysystemCodemountworks.
Separateuser-authorizedcode-fsforegroundsessionfreshA. First /btdrv returned
CodeUnavailable raw0xdd6ee202. STOPPEDagain,btmNOTattempted,nootherfallback.
RawResultmodule2,description6001 via13bitmask iswithinFSInvalidArgumentrange
(pinnedfs_results.hpp:431-435),upperbits0x375notinterpreted. Nativeexactfailedstage
notlogged,couldbeearliermetadataormount; doNOTlabelpermissiondenialorclaimtype0
validforALLservicecontextsjustfromenum. No speculativeAPIparameterretry.
Archive native-code-filesystem-20261020T064556Z report+session.log. 0NSOsaved.
Hostsentquit,NROREMOTEFILESESSIONENDresult8,REMOTEIDLEcleanup1,Finished. Hostpeer
connectionresetafterexpectedclose,serveexit1; NOTproofconsolecrash.

User'itexplodedagain';immediatelystopped,askedwhatshowing. Clarified TWICE:
shortgreenflashthenhbmenu,NOactualsegfault/fatal. Toldlogshowsweexplicitlyquit,
returnexpected;greenflashaloneNOTevidencecrash. No Atmospherefatalreportknown,
no menuslownessreported. UsernetloaderonwasNOTtreatedpermissionmoreattempts.
NormalprobeSDdiagnosticlogwasupdated; no module/configINI/93IPS/bootfileswritten,
noFTP/reinstall/rollback/BTinit/scan/connect/cancel/processcontrol/rawNAND/keys.
BothhostcoordinatorsendedandownUnixsocketscleaned; no backgroundsessionrunning.

ManifeststateNATIVE CODE ACCESS BLOCKED: BOTH READ-ONLY EXPORT ROUTES FAILED;
NO RADIO RETRY. Containsroutes/decodederrors/operatorclarification/exportedNSO0.
ActualnativeBTDRV/BTMscanresponsecode NOT analyzed becauseNOTobtained. No safe
source-backeddiscoveryfixestablished. Need legitimate safe access to the two
/mainNSOs (e.g. operatoralreadyhasdecryptedNSOsfromown21.2firmwaredump),NOTkeys,
fullNAND,unrelatedfilesoronlinecopyrightfirmwaredownload. Don'taskfornewdump/raw
storagetoolsasautomaticnextstep,don'tdebugattach/memorydumponunownedprocesses
orchangeprivileges/closeothersessions. No more pairing/netloaderbatcheswithout
new evidence-backedplan. Source-onlyresearchevidenceandfixturesremainavailable.

## 43. User requests host-first Mac/Docker plan; portable runner now implemented

User: research,usemacOSBluetoothorDockerifneeded,getcoderunninghere,thenportwith
fullperspectiveplan. Acknowledgedpositivepivot:validatecontrollerprotocolonMac,
shareexistingC++core,keepexplicitSwitchdiscovery/connectiongate. No more firmware
extraction prerequisite. Toldnopairing/FTPuntiltestedhostrunnerready.

Researchagents(readonly/noHW) completed:
- e5efc045-e973-4a7: GoogletestedmacOS13+Steam,SDLStadiaHIDAPIbackend+HIDAPI
  IOHIDDeviceInputReport callback usablebytepath. ApplecentralGATTAPIsdocumented,
  butNOauthoritativeMac-specificguarantee1812visible. iOSfilteredHIDclaimsNOT
  extrapolatedtoMac. OS-HID reportbytesarenot2A4D/2908proofofrawGATTaccess.
- e49c4a2e-6cf4-4d9: officialDockerLinuxVM,privilegednotMacCoreBluetooth,
  hostnetworkL4notdevicepass;USBIPconditionalexternaldevicecompatibilityNOT
  built-inadapterguarantee. BlueZneedsrealLinuxcontroller+DBus.
ParentverifiedDockerUSBIP,Googlecompatibility,HIDAPIcallbacksource(primaryURLs
inplan). No need addBleak/HIDAPIdependency; nativeIOKitCAPIavailableandbuilds.

Plan5gates in tools/STADIA-HOST-FIRST-PLAN.md:
1offlineportableHID/modelledGATTreplay;2nativeIOHIDactualstate;
3separateboundedCoreBluetoothGATTcapability;4sharedasyncsessionprotocolcore;
5Switch-specificownedconnect/discovery/subscription+emulatedinputverticalslice.
NoSwifttranspilingframeworkcalls. CoreBluetoothopaqueIDsNOTATT/MAC; don'tforge
handles inexistingrouterandclaimcapture. SubscriptionsneedactualACK,notIPCaccept.
SuccessMacdoesNOTpopulateSwitchcache/explain0x1a/security. Existing30fixtures
andSwiftIOHIDcapturealreadyprovepayloadgrammar; nofullcaptureexerciseagain.

Implementedtools/stadia-host noexternaldeps:
- source/decoder.hpp HidDecoderreadsproductionstadia_report.hpp+ReportLayout,
  validatesreportcallbackID3+prefix+11bytes+descriptor;Closeinvalidatesinput.
  NormalizesaxesviaSAMEproductionStadiaAxisToSwitch. GattFixtureModelseparate
  clearlyMODELLEDreference/endpoints/subscription,neverlivebackend.
- replay.hpp boundedfiles/map/rows+strictframing;stateJSONonly,generatedrowindex
  insteaduntrustedlabels,no packethex/addresses/serials. Terminalneutralstate
  onsuccessANDabortpreventsheldbuttons inUI.
- main.cpp --help/--replay no HIDframeworkinit;onlyexplicit--hid[--seconds1..180]
  callsbackend. Defaultusageexit2,invaliddurationrefusesbeforedeviceaccess.
- native_hid.cpp macOSnonseizingIOHIDManagerONLY18d1:9400,Bluetooth/BluetoothLow
  Energy,primaryGamePadusage. OSdescriptorcheckedbeforeinput,oneunique device;
  rejectsambiguity/removal/malformed. No keyboard,outputreport,seize,CBscan/pair,
  connect/disconnect,bond/globalBTwrites,rawpayloadorOSidentitylogging.
  Boundedsteady_clocktimeout,closesOWNmanageronly,emitsneutralonremoval/end.
  Linuxstubexplicitunsupportedlivebackend,notfakeBLEcontrolleraccess.
- build.sh linksIOKit+CoreFoundationonlyonMac;test.sh nativecompilerASan/UBSan+
  CLIprivacychecks, no valid--hidrun. tests/stadia_host_tests.cpp30states,
  IDprefix/payload03ambiguity,closed/invalidmap,modeldiff,hexlimits.
  test_cli.py malformedfixtures/duration/unknownflags/private-labelnoecho,
  heldbuttonfinal/abortneutral,source-scopednonseizingcontracts.
- README.md usage/evidenceboundaries,localbuildignored.gitignore.
Full tools/test-stadia.sh nowincludesnewhosttest.sh;tools/READMEpointeradded.

Independentreadonlyreview32c06c68-4ae9-43dfound2issues:arbitraryfixturelabel
couldleakidentifier/rawhex despitecharfilter, andreplaydidn'tneutralizefinalstate.
Bothfixed:generatedrowIDsandneutralterminal/abortrecords+targetedtests. Review
cannotcertifyactualIOHIDcallbacks/framing/ordering, no liveclaim. Parentverified
changesandreranALLprojecttests+nativeMacreleasebuild+DockerLinuxhosttestsPASS.
AllASan/UBSan clean;gitdiffcheckPASS. No suppressedfailingtests/newdependencies.
No productionSwitchheaders/adapterchanged, no Switchbuild/deployneeded.

Hostbinarytools/stadia-host/build/stadia-host62216bytesSHA256
b3f69b00cd29e9af8474b5e53c40ead0727466efb1a1819c075db9a39217407c.
Archive~/.cache/missioncontrol/stadia-host/host-first-20261020T072111Z/manifest.json
recordscompiler/sourcehashes/tests/limits+binarycopy. No HIDopened/Bluetooth/
controller/systempermission/FTP/netloader/SDmodulechangesduringthisturn.
LiveIOHIDandrawGATTnotvalidated. Nativebinaryisready,notcurrentlyrunning.

NEXToperatorstep:MacBluetoothON,pairONEStadiatoMacviaSystemSettings,USBunplugged;
askreadywhenmacOSsaysConnected. LeaveSwitchalone/noFTP/netloader/reboot.
ThenONEexplicit30sec:
tools/stadia-host/build/stadia-host --hid --seconds 30
Userpress/releasesomebuttons/movesticks;no full23phasefixturecaptureagain.
Ifaccessdenied,reportactualIOReturnandaskmanualInputMonitoringifneeded,don't
changepermissions/runroot/retryblindly. Distinguishreceivedneutralreportfrom
actualchangedinput;successrequireschangesforcontrollerbehaviorclaim. No rumble.
ThisisMacOSHIDpath+sharedC++proofONLY; don'tannounceSwitchsupportorrawGATT.

## 44. First LIVE macOS HID -> production C++ input path succeeded

UserBluetoothON/StadiaconnectedtoMac. AnnouncedONE30sec--hidrun,requestA/Band
bothsticks. Verifiedexactnativebinary62216SHAb3f69...17407c. Ranforegroundwith
timeout40s,normalizedstateJSONonly(no rawreports/privateidentifiers). Exit0.
Oneattached,343validreports,340statechanges,0rejected,removedfalse. OSdescriptor+
callbackID/prefixed11byteframingvalidated. Bothsticksrangecrossneutral:
LX0..4095,LY0..4078,RX0..4095,RY0..4095. LASTACTUALinputwasneutral(allbuttons0,
axes2047,triggers0,dpad8),thencompletionneutraltoo;notmerelysyntheticendreset.
No device-removal/disconnecttest; no claimthatpathvalidated.

UserthenclarifiedprecisesequenceA+B,leftstickaround,rightstickaround,A,B.
Privatelysummarizedbuttontransitions:initialB->A+B->B->released,
thenattheendA->released->B->released. Intermediate340staterowscapturestick
movement. Thismatchesphysicalpress/releaseoverlap;don'tdescribeasextraunaskedB
failureorwrongmapping. No needrepeatfullbuttoncapture:30fixturesalreadyexisted.

Archive~/.cache/missioncontrol/stadia-host/live-hid-20261020T073936Z/
state.jsonl/diagnostic.log/manifest.json. Manifestrecordsoperatorsequence,
A_plus_B_seen,axisranges,lastactualneutral,stateSHA,evidenceflags. Tooloutput
showedmanynormalizedstates(truncated50KB)butfullarchivevalidated;futuretools
shouldprintSUMMARYonlyratherthandumpingstate.jsonl. Noaddresses/keys/rawpayloads.
HostruntimeclosedonlyOWNIOHIDmanager;OSBluetoothlinkleftalone. No pairing/connect/
notification/outputwrite/permissionchange/BTsetting/Switchrequestfromrunner.

Toldusersuccessprecisely:Stadia->macOSOSHID->sameC++decode, NOTrawGATTorSwitch.
Plan/READMEupdatedstep2evidence. NexttaskimplementboundedCoreBluetoothcapability
checkperplanstep3:discoveractual1812/map/refsifavailable,notassumefromOSHID.
CurrentMacstillmayconnected(operatorcontrols);don'tassumeavailabilityisnewtest
permission. Needscope+testedtool+readybeforeCBclientconnect/read/subscribe.
No extraSwitchopsneeded. No process/backgroundcapturestillrunning.


















