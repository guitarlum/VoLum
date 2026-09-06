// Update-notifier orchestration. This file is tail-included by
// NeuralAmpModeler.cpp so the detached worker never needs a plugin pointer.

#include "VoLumHttpGet.h"

namespace
{

constexpr const char* kVolumAppcastUrl = "https://guitarlum.github.io/VoLum/appcast.json";
constexpr int kVolumUpdateTimeoutMs = 3500;
std::atomic<bool> gVolumAutomaticUpdateCheckStarted{false};

std::int64_t VolumUtcNow()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

void NeuralAmpModeler::_VolumLoadUpdateState()
{
  if (mVolumUpdateStateLoaded)
    return;
  mVolumUpdateState = volum::update::LoadUpdateState(volum::VolumUpdateStateFilePath());
  mVolumUpdateStateLoaded = true;
}

void NeuralAmpModeler::_VolumStartUpdateCheck(bool manual)
{
  _VolumLoadUpdateState();
  if (mVolumUpdateCheckInFlight || (!manual && !mVolumUpdateState.autoCheck))
    return;

  const auto now = VolumUtcNow();
  if (!manual && !volum::update::FakeUpdateRequested())
  {
    if (!volum::update::ShouldCheck(now, mVolumUpdateState.lastCheckUtc))
      return;
    bool expected = false;
    if (!gVolumAutomaticUpdateCheckStarted.compare_exchange_strong(expected, true))
      return;
  }

  const auto statePath = volum::VolumUpdateStateFilePath();
  auto result = std::make_shared<volum::update::AsyncResult>();
  mVolumUpdateResult = result;
  mVolumUpdateCheckInFlight = true;
  const std::string currentVersion = PLUG_VERSION_STR;

  if (volum::update::FakeUpdateRequested())
  {
    // UAT-only: never persist, never hit the network. A sidecar write would
    // leave a phantom 2.0.0 on the next real launch.
    result->manifest = volum::update::FakeUpdateManifest();
    result->succeeded = true;
    result->complete.store(true, std::memory_order_release);
    return;
  }

  std::thread([result, statePath, currentVersion, now]() {
    // Persist the attempt before network I/O. Offline machines are therefore
    // throttled too, and concurrent plugin instances only risk one harmless
    // last-writer-wins update to this dedicated sidecar.
    auto state = volum::update::LoadUpdateState(statePath);
    state.lastCheckUtc = now;
    volum::update::SaveUpdateState(statePath, state);

    std::string response;
    volum::update::Manifest manifest;
    if (VolumHttpGetString(kVolumAppcastUrl, response, kVolumUpdateTimeoutMs)
        && volum::update::ParseManifest(response, manifest))
    {
      state = volum::update::LoadUpdateState(statePath);
      volum::update::ApplyCheckedManifest(state, manifest, currentVersion, now);
      volum::update::SaveUpdateState(statePath, state);
      result->manifest = std::move(manifest);
      result->succeeded = true;
    }
    result->complete.store(true, std::memory_order_release);
  }).detach();
}

void NeuralAmpModeler::_VolumConsumeUpdateResult()
{
  if (!mVolumUpdateCheckInFlight || !mVolumUpdateResult
      || !mVolumUpdateResult->complete.load(std::memory_order_acquire))
    return;

  const bool succeeded = mVolumUpdateResult->succeeded;
  const auto manifest = mVolumUpdateResult->manifest;
  mVolumUpdateResult.reset();
  mVolumUpdateCheckInFlight = false;

  if (volum::update::FakeUpdateRequested() && succeeded)
  {
    // Stay in memory. Writing 2.0.0 into the sidecar would haunt the next real launch.
    mVolumUpdateState.latestKnownVersion = manifest.version;
    mVolumUpdateState.latestKnownUrl = manifest.url;
    mVolumUpdateState.latestKnownNotes = manifest.notes;
  }
  else
    mVolumUpdateState = volum::update::LoadUpdateState(volum::VolumUpdateStateFilePath());

  const volum::update::BadgeState badge{mVolumUpdateState.latestKnownVersion, mVolumUpdateState.lastSeenVersion};
  if (succeeded && volum::update::ShouldShowBadge(badge, PLUG_VERSION_STR))
    mVolumUpdateFooterTicks = 180;
  _VolumRefreshUpdateUi();
}

void NeuralAmpModeler::_VolumRefreshUpdateUi()
{
  auto* pGraphics = GetUI();
  if (!pGraphics)
    return;

  const volum::update::BadgeState badgeState{mVolumUpdateState.latestKnownVersion, mVolumUpdateState.lastSeenVersion};
  const bool available = volum::update::IsUpdateAvailable(badgeState, PLUG_VERSION_STR);
  if (auto* badge = pGraphics->GetControlWithTag(kCtrlTagVoLumUpdateBadge))
    badge->Hide(!volum::update::ShouldShowBadge(badgeState, PLUG_VERSION_STR));
  if (auto* settings = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
    settings->As<NAMSettingsPageControl>()->SetUpdateInfo(
      mVolumUpdateState.autoCheck, available, mVolumUpdateState.latestKnownVersion, mVolumUpdateState.latestKnownNotes);
}

void NeuralAmpModeler::_VolumCheckForUpdatesNow()
{
  _VolumLoadUpdateState();
  volum::update::BadgeState badge{mVolumUpdateState.latestKnownVersion, mVolumUpdateState.lastSeenVersion};
  volum::update::MarkSeen(badge);
  mVolumUpdateState.lastSeenVersion = badge.lastSeenVersion;
  volum::update::SaveUpdateState(volum::VolumUpdateStateFilePath(), mVolumUpdateState);
  _VolumRefreshUpdateUi();
  _VolumStartUpdateCheck(true);
}

void NeuralAmpModeler::_VolumSetAutoUpdateCheck(bool enabled)
{
  _VolumLoadUpdateState();
  mVolumUpdateState.autoCheck = enabled;
  volum::update::SaveUpdateState(volum::VolumUpdateStateFilePath(), mVolumUpdateState);
  _VolumRefreshUpdateUi();
  if (enabled)
    _VolumStartUpdateCheck(false);
}

void NeuralAmpModeler::_VolumUseAvailableUpdate()
{
  _VolumLoadUpdateState();
  volum::update::BadgeState badge{mVolumUpdateState.latestKnownVersion, mVolumUpdateState.lastSeenVersion};
  volum::update::MarkSeen(badge);
  mVolumUpdateState.lastSeenVersion = badge.lastSeenVersion;
  volum::update::SaveUpdateState(volum::VolumUpdateStateFilePath(), mVolumUpdateState);
  _VolumRefreshUpdateUi();

  if (!mVolumUpdateState.latestKnownUrl.empty())
    if (auto* pGraphics = GetUI())
      pGraphics->OpenURL(mVolumUpdateState.latestKnownUrl.c_str());
}
