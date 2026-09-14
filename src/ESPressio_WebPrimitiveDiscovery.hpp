#pragma once

#if !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Web Primitive discovery requires ESPressio-Primitive."
#endif

#include <cstddef>
#include <string_view>

#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_WebTypes.hpp"

namespace ESPressio::Web {

/// <summary>
/// Read-only Web/tooling adapter over a frozen Primitive TypeDirectoryView.
/// </summary>
/// <remarks>
/// This adapter owns no Primitive descriptors, performs no family behavior and
/// never interprets PrimitiveFamilyExtensionRef. It exists only to expose the
/// final immutable Primitive discovery surface to dynamic Web tooling. Family
/// policy, schema interpretation, construction, admission and authorization
/// remain owned by the corresponding family/Serializable layers.
/// </remarks>
class PrimitiveTypeDiscovery final {
public:
    /// <summary>
    /// Binds a frozen caller-owned Primitive directory view.
    /// </summary>
    /// <remarks>
    /// The directory owner must outlive this adapter. An unavailable/non-frozen
    /// view is rejected so dynamic tooling cannot begin before topology freeze.
    /// </remarks>
    WebResult Bind(Primitive::TypeDirectoryView directory) noexcept {
        if (!directory.IsFrozen()) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        _directory = directory;
        _bound = true;
        return WebResult::Success();
    }

    /// <summary>Returns whether a frozen directory has been bound.</summary>
    bool IsBound() const noexcept { return _bound; }

    /// <summary>Returns the number of discoverable descriptors.</summary>
    std::size_t Count() const noexcept {
        return _bound ? _directory.Size() : 0;
    }

    /// <summary>
    /// Returns the descriptor at deterministic frozen-directory enumeration index.
    /// </summary>
    /// <remarks>
    /// The returned pointer is borrowed from the caller-owned TypeDirectory.
    /// </remarks>
    const Primitive::PrimitiveTypeDescriptor* At(std::size_t index) const noexcept {
        if (!_bound || index >= _directory.Size()) return nullptr;
        return _directory.begin() + index;
    }

    /// <summary>Finds a descriptor by exact family-qualified semantic Type key.</summary>
    const Primitive::PrimitiveTypeDescriptor* Find(
        Primitive::PrimitiveTypeKey key
    ) const noexcept {
        return _bound ? _directory.Find(key) : nullptr;
    }

    /// <summary>
    /// Finds a descriptor by exact case-sensitive canonical name within one family.
    /// </summary>
    const Primitive::PrimitiveTypeDescriptor* Find(
        Primitive::PrimitiveFamilyId family,
        Primitive::PrimitiveTypeNameView canonicalName
    ) const noexcept {
        return _bound ? _directory.Find(family, canonicalName) : nullptr;
    }

    /// <summary>
    /// Returns the immutable borrowed directory view for a family-specific adapter.
    /// </summary>
    /// <remarks>
    /// Consumers must still validate family identity before interpreting a family
    /// extension and must not treat descriptor availability as authorization.
    /// </remarks>
    Primitive::TypeDirectoryView Directory() const noexcept {
        return _bound ? _directory : Primitive::TypeDirectoryView{};
    }

private:
    Primitive::TypeDirectoryView _directory{};
    bool _bound = false;
};

} // namespace ESPressio::Web
