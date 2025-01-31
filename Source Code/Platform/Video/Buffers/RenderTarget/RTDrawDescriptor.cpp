#include "stdafx.h"

#include "Headers/RTDrawDescriptor.h"

namespace Divide {

bool IsEnabled(const RTDrawMask& mask, const RTAttachmentType type) noexcept {
    switch (type) {
        case RTAttachmentType::Depth:   return !mask._disabledDepth;
        case RTAttachmentType::Stencil: return !mask._disabledStencil;
        case RTAttachmentType::Colour: {
            for (const bool state : mask._disabledColours) {
                if (!state) {
                    return true;
                }
            }
            return false;
        }
        default: break;
    }

    return true;
}

bool IsEnabled(const RTDrawMask& mask, const RTAttachmentType type, const U8 index) noexcept {
    assert(index < MAX_RT_COLOUR_ATTACHMENTS);

    if (type == RTAttachmentType::Colour) {
        return !mask._disabledColours[index];
    }
     
    return IsEnabled(mask, type);
}

void SetEnabled(RTDrawMask& mask, const RTAttachmentType type, const U8 index, const bool state) noexcept {
    assert(index < MAX_RT_COLOUR_ATTACHMENTS);

    switch (type) {
        case RTAttachmentType::Depth   : mask._disabledDepth   = !state; break;
        case RTAttachmentType::Stencil : mask._disabledStencil = !state; break;
        case RTAttachmentType::Colour  : mask._disabledColours[index] = !state; break;
        default : break;
    }
}

void EnableAll(RTDrawMask& mask) {
    mask._disabledDepth = mask._disabledStencil = false;
    mask._disabledColours.fill(false);
}

void DisableAll(RTDrawMask& mask) {
    mask._disabledDepth = mask._disabledStencil = true;
    mask._disabledColours.fill(true);
}

}; //namespace Divide