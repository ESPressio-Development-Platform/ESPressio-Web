#include <cassert>
#include <cstddef>
#include <cstdint>

#include <ESPressio_PrimitiveFamilyRegistry.hpp>
#include <ESPressio_WebPrimitiveDiscovery.hpp>

using namespace ESPressio;
using namespace ESPressio::Web;

namespace {

Primitive::PrimitiveTypeDescriptor Descriptor(
    Primitive::PrimitiveFamilyId family,
    std::uint64_t typeValue,
    Primitive::PrimitiveTypeNameView name
) {
    Primitive::PrimitiveTypeDescriptor descriptor{};
    descriptor.Key = {family, typeValue};
    descriptor.CanonicalName = name;
    descriptor.Capabilities = Primitive::PrimitiveTypeCapabilities{};
    descriptor.Versions = {1, 1};
    return descriptor;
}

void TestRejectsUnavailableDirectory() {
    PrimitiveTypeDiscovery discovery;
    assert(!discovery.IsBound());
    assert(discovery.Count() == 0);
    assert(discovery.At(0) == nullptr);
    assert(discovery.Find({Primitive::FamilyIds::Command, 1}) == nullptr);

    const auto result = discovery.Bind(Primitive::TypeDirectoryView{});
    assert(!result);
    assert(result.Error == WebError::InvalidConfiguration);
    assert(!discovery.IsBound());
}

void TestConsumesFrozenDirectoryDeterministically() {
    Primitive::TypeDirectory<4> directory;

    // Deliberately register out of semantic order. The frozen directory owns
    // deterministic {Family, TypeId} ordering, not the Web adapter.
    assert(directory.Register(Descriptor(
        Primitive::FamilyIds::Event,
        9,
        "ButtonPressed"
    )) == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Register(Descriptor(
        Primitive::FamilyIds::Command,
        7,
        "RestartMotor"
    )) == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Register(Descriptor(
        Primitive::FamilyIds::State,
        3,
        "BatteryLevel"
    )) == Primitive::TypeDirectoryRegistrationStatus::Success);

    assert(directory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);

    PrimitiveTypeDiscovery discovery;
    assert(discovery.Bind(directory.View()));
    assert(discovery.IsBound());
    assert(discovery.Count() == 3);

    const auto* first = discovery.At(0);
    const auto* second = discovery.At(1);
    const auto* third = discovery.At(2);
    assert(first != nullptr && second != nullptr && third != nullptr);
    assert(first->Key.Family == Primitive::FamilyIds::Command);
    assert(first->Key.TypeValue == 7);
    assert(second->Key.Family == Primitive::FamilyIds::Event);
    assert(second->Key.TypeValue == 9);
    assert(third->Key.Family == Primitive::FamilyIds::State);
    assert(third->Key.TypeValue == 3);
    assert(discovery.At(3) == nullptr);
}

void TestExactDiscoveryOnly() {
    Primitive::TypeDirectory<3> directory;
    assert(directory.Register(Descriptor(
        Primitive::FamilyIds::Command,
        42,
        "RestartMotor"
    )) == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Register(Descriptor(
        Primitive::FamilyIds::Event,
        42,
        "RestartMotor"
    )) == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);

    PrimitiveTypeDiscovery discovery;
    assert(discovery.Bind(directory.View()));

    const auto* commandByKey = discovery.Find({Primitive::FamilyIds::Command, 42});
    assert(commandByKey != nullptr);
    assert(commandByKey->CanonicalName == "RestartMotor");

    const auto* commandByName = discovery.Find(
        Primitive::FamilyIds::Command,
        "RestartMotor"
    );
    assert(commandByName == commandByKey);

    const auto* eventByName = discovery.Find(
        Primitive::FamilyIds::Event,
        "RestartMotor"
    );
    assert(eventByName != nullptr);
    assert(eventByName->Key.Family == Primitive::FamilyIds::Event);

    assert(discovery.Find(
        Primitive::FamilyIds::Command,
        "restartmotor"
    ) == nullptr);
    assert(discovery.Find({Primitive::FamilyIds::Command, 999}) == nullptr);

    // The adapter returns only the immutable common descriptor. It performs no
    // family-extension interpretation and no family behavior/admission.
    assert(discovery.Directory().IsFrozen());
    assert(discovery.Directory().Size() == 2);
}

} // namespace

int main() {
    TestRejectsUnavailableDirectory();
    TestConsumesFrozenDirectoryDeterministically();
    TestExactDiscoveryOnly();
    return 0;
}
