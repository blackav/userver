#include "component_context_component_info.hpp"

#include <components/component_context.hpp>
#include <logging/log.hpp>
#include <tracing/tracer.hpp>

namespace {
const std::string kComponentName = "component_name";
const std::string kStopComponentRootName = "component_stop";
const std::string kOnAllComponentsAreStopping =
    "on_all_components_are_stopping";
}  // namespace

namespace components {
namespace impl {

StageSwitchingCancelledException::StageSwitchingCancelledException(
    std::string message)
    : std::runtime_error(std::move(message)) {}

ComponentInfo::ComponentInfo(std::string name) : name_(std::move(name)) {}

void ComponentInfo::SetComponent(std::unique_ptr<ComponentBase>&& component) {
  bool call_on_loading_cancelled = false;
  {
    std::lock_guard<engine::Mutex> lock(mutex_);
    component_ = std::move(component);
    stage_ = ComponentLifetimeStage::kCreated;
    if (stage_switching_cancelled_) call_on_loading_cancelled = true;
  }
  if (call_on_loading_cancelled) OnLoadingCancelled();
  cv_.NotifyAll();
}

void ComponentInfo::ClearComponent() {
  if (!HasComponent()) return;
  tracing::Span span(kStopComponentRootName);
  span.AddTag(kComponentName, name_);

  auto component = ExtractComponent();
  LOG_INFO() << "Stopping component";
  component.reset();
  LOG_INFO() << "Stopped component";
}

ComponentBase* ComponentInfo::GetComponent() const {
  std::lock_guard<engine::Mutex> lock(mutex_);
  return component_.get();
}

ComponentBase* ComponentInfo::WaitAndGetComponent() const {
  std::unique_lock<engine::Mutex> lock(mutex_);
  auto ok = cv_.Wait(lock, [this]() {
    return stage_switching_cancelled_ || component_.get() != nullptr;
  });
  if (!ok || stage_switching_cancelled_)
    throw ComponentsLoadCancelledException();
  return component_.get();
}

void ComponentInfo::AddItDependsOn(std::string component) {
  std::lock_guard<engine::Mutex> lock(mutex_);
  it_depends_on_.insert(std::move(component));
}

void ComponentInfo::AddDependsOnIt(std::string component) {
  std::lock_guard<engine::Mutex> lock(mutex_);
  depends_on_it_.insert(std::move(component));
}

bool ComponentInfo::CheckItDependsOn(const std::string& component) const {
  std::lock_guard<engine::Mutex> lock(mutex_);
  return it_depends_on_.find(component) != it_depends_on_.end();
}

bool ComponentInfo::CheckDependsOnIt(const std::string& component) const {
  std::lock_guard<engine::Mutex> lock(mutex_);
  return depends_on_it_.find(component) != depends_on_it_.end();
}

void ComponentInfo::SetStageSwitchingCancelled(bool cancelled) {
  {
    std::lock_guard<engine::Mutex> lock(mutex_);
    stage_switching_cancelled_ = cancelled;
  }
  cv_.NotifyAll();
}

void ComponentInfo::OnLoadingCancelled() {
  if (!HasComponent()) return;
  if (on_loading_cancelled_called_.exchange(true)) return;
  component_->OnLoadingCancelled();
}

void ComponentInfo::OnAllComponentsLoaded() {
  if (!HasComponent()) return;
  try {
    component_->OnAllComponentsLoaded();
  } catch (const std::exception& ex) {
    std::string message = "OnAllComponentsLoaded() failed for component " +
                          name_ + ": " + ex.what();
    LOG_ERROR() << message;
    throw std::runtime_error(message);
  }
}

void ComponentInfo::OnAllComponentsAreStopping() {
  if (!HasComponent()) return;
  try {
    tracing::Span span(kOnAllComponentsAreStopping);
    component_->OnAllComponentsAreStopping();
  } catch (const std::exception& ex) {
    LOG_ERROR() << "OnAllComponentsAreStopping() failed for component " << name_
                << ": " << ex;
  }
}

void ComponentInfo::SetStage(ComponentLifetimeStage stage) {
  {
    std::lock_guard<engine::Mutex> lock(mutex_);
    stage_ = stage;
  }
  cv_.NotifyAll();
}

ComponentLifetimeStage ComponentInfo::GetStage() const {
  std::lock_guard<engine::Mutex> lock(mutex_);
  return stage_;
}

void ComponentInfo::WaitStage(ComponentLifetimeStage stage,
                              std::string method_name) const {
  std::unique_lock<engine::Mutex> lock(mutex_);
  auto ok = cv_.Wait(lock, [this, stage]() {
    return stage_switching_cancelled_ || stage_ == stage;
  });
  if (!ok && stage_switching_cancelled_)
    throw StageSwitchingCancelledException(
        std::move(method_name.append(" cancelled")));
}

bool ComponentInfo::HasComponent() const {
  std::lock_guard<engine::Mutex> lock(mutex_);
  return !!component_;
}

std::unique_ptr<ComponentBase> ComponentInfo::ExtractComponent() {
  std::unique_ptr<ComponentBase> component;
  {
    std::lock_guard<engine::Mutex> lock(mutex_);
    std::swap(component, component_);
  }
  return component;
}

}  // namespace impl
}  // namespace components
