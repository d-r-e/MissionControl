#include "../mc_mitm/source/bluetooth_mitm/bluetooth/ble_connection_lease.hpp"
#include "../mc_mitm/source/bluetooth_mitm/bluetooth/ble_managed_disconnect_ack.hpp"
#include <cassert>
#include <limits>
#include <type_traits>

using namespace ams::bluetooth::ble_connection;
namespace {
constexpr Address Target{1, 2, 3, 4, 5, 6}, Other{6, 5, 4, 3, 2, 1};
constexpr std::uint64_t Owner = 42;
constexpr std::uint8_t Client = 7;
void Arm(ConnectionLease &lease) { assert(lease.Arm(Target, Owner, 100, 50)); }
void Capture(ConnectionLease &lease) {
    assert(lease.BeforeConnect(Owner, Target, Client, true) == ConnectDisposition::Captured);
}
void Start(ConnectionLease &lease) { Arm(lease); Capture(lease); lease.AfterConnect(true); }
void Event(ConnectionLease &lease, std::uint32_t status, std::uint32_t id = 123) {
    lease.OnConnection(Target, Client, true, status, id);
}
void None(ConnectionLease &lease) { assert(lease.TakeAction().type == ActionType::None); }
void ActionIs(ConnectionLease &lease, ActionType type) {
    const auto action = lease.TakeAction();
    assert(action.type == type && action.address == Target && action.client_if == Client);
    assert(action.is_direct);
    assert(action.conn_id == (type == ActionType::Disconnect ? 123 : 0xffffffffu));
    None(lease); // Reservation, not repeatable polling.
}
void ValidationAndIsolation() {
    static_assert(!std::is_copy_constructible_v<ConnectionLease>);
    static_assert(!std::is_move_constructible_v<ConnectionLease>);
    ConnectionLease lease;
    lease.Stop(); lease.Tick(999); lease.AfterConnect(true); lease.CompleteAction(false);
    Event(lease, 0); None(lease);
    assert(lease.GetSnapshot().state == State::Idle);
    assert(lease.BeforeConnect(Owner, Target, Client, true) == ConnectDisposition::Unrelated);
    assert(!lease.Arm({}, Owner, 0, 50));
    assert(!lease.Arm(Address{255, 255, 255, 255, 255, 255}, Owner, 0, 50));
    assert(!lease.Arm(Target, 0, 0, 50));
    assert(!lease.Arm(Target, Owner, 0, 0));
    assert(!lease.Arm(Target, Owner, 0, 30001));
    assert(!lease.Arm(Target, Owner, std::numeric_limits<std::uint64_t>::max(), 1));
    Arm(lease);
    assert(!lease.Arm(Other, Owner, 100, 50));
    assert(lease.BeforeConnect(Owner, Other, Client, false) == ConnectDisposition::Unrelated);
    assert(lease.BeforeConnect(99, Other, Client, false) == ConnectDisposition::Unrelated);
    Event(lease, 0); None(lease); // No tuple captured yet.
    Capture(lease);
    lease.OnConnection(Other, Client, true, 0, 123);
    lease.OnConnection(Target, Client + 1, true, 0, 123);
    lease.OnConnection(Target, Client, true, 99, 123);
    lease.AfterConnect(true);
    assert(lease.GetSnapshot().state == State::Pending);
    None(lease);
}
void TimeoutAndTerminal() {
    ConnectionLease lease; Start(lease);
    lease.Tick(149); None(lease);
    lease.Tick(150);
    assert(lease.GetSnapshot().reason == Reason::TimedOut);
    ActionIs(lease, ActionType::Cancel);
    lease.CompleteAction(true);
    assert(lease.GetSnapshot().state == State::Stopping); // IPC acceptance is not terminal.
    lease.Tick(1000); lease.Stop(); None(lease);
    Event(lease, 0); // Late connect after accepted cancel.
    ActionIs(lease, ActionType::Disconnect);
    lease.CompleteAction(true);
    assert(lease.GetSnapshot().state == State::Stopping);
    Event(lease, 2, 999); // Not the known owned connection.
    assert(lease.GetSnapshot().state == State::Stopping);
    Event(lease, 2);
    assert(lease.GetSnapshot().state == State::Terminal);
    Event(lease, 0); Event(lease, 2); None(lease);
    assert(lease.GetSnapshot().state == State::Quarantined);
    assert(lease.GetSnapshot().reason == Reason::PostTerminalEvent);
    assert(!lease.Arm(Target, Owner, 200, 10));
    assert(lease.BeforeConnect(Owner, Target, Client, true) == ConnectDisposition::Rejected);
    assert(lease.GetSnapshot().actions_issued == 2);
}
void StopBeforeCapture() {
    for (bool timeout : {false, true}) {
        ConnectionLease lease; Arm(lease);
        if (timeout) lease.Tick(150); else lease.Stop();
        None(lease); // No unknown cancellation tuple.
        Capture(lease); None(lease); // Not yet accepted.
        lease.AfterConnect(true);
        ActionIs(lease, ActionType::Cancel);
        lease.CompleteAction(true); None(lease);
        Event(lease, 2);
        assert(lease.GetSnapshot().state == State::Terminal);
    }
    ConnectionLease lease; Arm(lease); lease.Stop(); Capture(lease);
    Event(lease, 0); None(lease); // Early success retained until IPC return.
    lease.AfterConnect(true);
    ActionIs(lease, ActionType::Disconnect);
}
void EarlyCallbacksAndOneShot() {
    ConnectionLease lease; Arm(lease); Capture(lease);
    Event(lease, 0); None(lease);
    lease.AfterConnect(true);
    assert(lease.GetSnapshot().state == State::Connected);
    Event(lease, 0); None(lease);
    assert(lease.BeforeConnect(Owner, Target, Client, true) == ConnectDisposition::Rejected);
    lease.Stop(); ActionIs(lease, ActionType::Disconnect);
    Event(lease, 0); None(lease);
    Event(lease, 2); // Terminal before action IPC returns.
    lease.CompleteAction(true); None(lease);
    assert(lease.GetSnapshot().state == State::Terminal);
    ConnectionLease terminal; Arm(terminal); Capture(terminal);
    Event(terminal, 2); terminal.AfterConnect(true); terminal.Stop(); None(terminal);
    assert(terminal.GetSnapshot().state == State::Terminal);
    ConnectionLease fresh; assert(fresh.Arm(Target, Owner, 0, 30000));
}
void ForeignAmbiguity() {
    for (int phase = 0; phase != 4; ++phase) {
        ConnectionLease lease; Arm(lease);
        if (phase > 0) Capture(lease);
        if (phase > 1) lease.AfterConnect(true);
        if (phase > 2) Event(lease, 0);
        assert(lease.BeforeConnect(99, Target, Client, true) == ConnectDisposition::Unrelated);
        assert(lease.GetSnapshot().ambiguous);
        assert(lease.GetSnapshot().state == State::Quarantined);
        assert(lease.BeforeConnect(Owner, Target, Client, true) == ConnectDisposition::Rejected);
        lease.AfterConnect(true); lease.Stop(); Event(lease, 0); None(lease);
    }
    ConnectionLease missing; Arm(missing);
    missing.BeforeConnect(0, Target, Client, false);
    assert(missing.GetSnapshot().ambiguous); None(missing);
    ConnectionLease in_flight; Start(in_flight); in_flight.Stop();
    ActionIs(in_flight, ActionType::Cancel);
    in_flight.BeforeConnect(99, Target, Client, false);
    in_flight.CompleteAction(true); Event(in_flight, 0); None(in_flight);
}
void FailedResults() {
    ConnectionLease failed; Arm(failed); Capture(failed); failed.AfterConnect(false);
    assert(failed.GetSnapshot().state == State::Quarantined); None(failed);
    failed.AfterConnect(true); None(failed); // Cannot overwrite uncertain return.
    assert(failed.BeforeConnect(Owner, Target, Client, true) == ConnectDisposition::Rejected);
    Event(failed, 0); // Precisely owned late success can still be disconnected.
    ActionIs(failed, ActionType::Disconnect); failed.CompleteAction(true); None(failed);
    assert(failed.GetSnapshot().state == State::Quarantined);
    Event(failed, 2); assert(failed.GetSnapshot().state == State::Terminal);

    ConnectionLease early; Arm(early); Capture(early); Event(early, 0);
    early.AfterConnect(false); ActionIs(early, ActionType::Disconnect);

    ConnectionLease cancel; Start(cancel); cancel.Stop(); ActionIs(cancel, ActionType::Cancel);
    cancel.CompleteAction(false); None(cancel);
    assert(cancel.GetSnapshot().action_failures == 1);
    Event(cancel, 0); ActionIs(cancel, ActionType::Disconnect);
    cancel.CompleteAction(false); Event(cancel, 0); cancel.Tick(900); None(cancel);
    assert(cancel.GetSnapshot().action_failures == 2);
    assert(cancel.GetSnapshot().state == State::Quarantined);
    Event(cancel, 2); assert(cancel.GetSnapshot().state == State::Terminal);

    ConnectionLease callback; Start(callback);
    callback.OnConnection(Target, Client, false, 2, 123); None(callback);
    assert(callback.GetSnapshot().state == State::Quarantined);
    Event(callback, 0); ActionIs(callback, ActionType::Disconnect);
}
void InvalidAndConflictingIds() {
    for (auto status : {0u, 2u}) {
        ConnectionLease lease; Start(lease);
        Event(lease, status, ConnectionLease::InvalidConnection); lease.Stop(); None(lease);
        assert(lease.GetSnapshot().state == State::Quarantined);
        assert(!lease.GetSnapshot().connection_known);
        Event(lease, 0); None(lease); // Do not infer ownership from inconsistent id evidence.
    }
    ConnectionLease conflict; Start(conflict); Event(conflict, 0);
    Event(conflict, 0, 456); conflict.Stop(); None(conflict);
    assert(conflict.GetSnapshot().ambiguous);
    ConnectionLease during_cancel; Start(during_cancel); during_cancel.Stop();
    ActionIs(during_cancel, ActionType::Cancel);
    Event(during_cancel, 0); None(during_cancel);
    during_cancel.CompleteAction(true);
    ActionIs(during_cancel, ActionType::Disconnect);
    during_cancel.CompleteAction(true); Event(during_cancel, 0); None(during_cancel);
}
void ManagedDisconnectAcknowledgements() {
    using D = ManagedDisconnectAck::Disposition;
    ManagedDisconnectAck unsolicited;
    assert(unsolicited.Record(true, false) == D::Reject);
    assert(unsolicited.Record(false, true) == D::Reject);
    assert(unsolicited.Issue());
    assert(!unsolicited.Issue());
    assert(unsolicited.Record(false, false) == D::Reject);
    assert(unsolicited.Record(true, false) == D::CompletePending);
    assert(unsolicited.Record(true, false) == D::Reject);
    assert(unsolicited.Record(false, true) == D::Reject);

    // Exercise both orders of the real hardware race with the lease itself.
    for (bool terminal_first : {false, true}) {
        ConnectionLease lease; Start(lease); Event(lease, 0); lease.Stop();
        ActionIs(lease, ActionType::Disconnect);
        ManagedDisconnectAck ack; assert(ack.Issue());
        if (terminal_first) {
            Event(lease, 2);
            lease.CompleteAction(true); // Event observer clears pending action.
        }
        const auto snapshot = lease.GetSnapshot();
        const auto disposition = ack.Record(snapshot.action_in_flight, snapshot.state == State::Terminal);
        assert(disposition == (terminal_first ? D::AlreadyTerminal : D::CompletePending));
        if (disposition == D::CompletePending) lease.CompleteAction(true);
        if (!terminal_first) Event(lease, 2);
        assert(lease.GetSnapshot().state == State::Terminal);
        assert(lease.GetSnapshot().actions_issued == 1);
        assert(lease.GetSnapshot().action_failures == 0);
        assert(ack.Record(false, true) == D::Reject); // No duplicate reports.
        None(lease);
    }
    ConnectionLease failed; Start(failed); Event(failed, 0); failed.Stop();
    ActionIs(failed, ActionType::Disconnect);
    ManagedDisconnectAck failed_ack; assert(failed_ack.Issue());
    assert(failed_ack.Record(true, false) == D::CompletePending);
    failed.CompleteAction(false);
    assert(failed.GetSnapshot().state == State::Quarantined);
    Event(failed, 2);
    assert(failed.GetSnapshot().state == State::Terminal);
    assert(failed_ack.Record(false, true) == D::Reject);
}
void ExactTupleAndDuplicates() {
    ConnectionLease lease; Arm(lease);
    assert(lease.BeforeConnect(Owner, Target, 0, false) == ConnectDisposition::Captured);
    lease.Stop(); lease.AfterConnect(true);
    const auto action = lease.TakeAction();
    assert(action.type == ActionType::Cancel && action.client_if == 0 && !action.is_direct);
    assert(action.address == Target && action.conn_id == ConnectionLease::InvalidConnection);
    lease.CompleteAction(true); lease.CompleteAction(false); None(lease);
    assert(lease.GetSnapshot().action_failures == 0);
    lease.OnConnection(Target, 0, true, 0, 0); // Zero is valid; only ffffffff is sentinel.
    const auto disconnect = lease.TakeAction();
    assert(disconnect.type == ActionType::Disconnect && disconnect.conn_id == 0);
    lease.CompleteAction(true);
    lease.OnConnection(Target, 0, true, 0, 0); None(lease);
    lease.OnConnection(Target, 0, true, 2, 0);
    assert(lease.GetSnapshot().state == State::Terminal);
}
} // namespace
int main() {
    ValidationAndIsolation(); TimeoutAndTerminal(); StopBeforeCapture();
    EarlyCallbacksAndOneShot(); ForeignAmbiguity(); FailedResults();
    InvalidAndConflictingIds(); ExactTupleAndDuplicates(); ManagedDisconnectAcknowledgements();
}
