/*
 * SoC Video Frame Capture HAL (SPECIFICATION-soc-video-frame-capture.md v0.9).
 *
 * The ICaptureSession interface (section 3) and value types (section 5.1)
 * shared by MW and the SoC vendor implementation, plus the vendor library
 * factory (section 8.1).
 *
 * The specification gives these names without a namespace; they are placed in
 * rdkcapture:: here so the generic names (Size, Rectangle) cannot collide with
 * MW types.
 */
#ifndef RDK_CAPTURE_ICAPTURESESSION_H
#define RDK_CAPTURE_ICAPTURESESSION_H

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace rdkcapture
{

enum class CaptureStatus : uint32_t
{
    OK,
    NO_NEW_FRAME,
    UNSUPPORTED,
    INVALID_ARGUMENT,
    NO_RESOURCES,
    NO_FRAME,
    FATAL_ERROR,
};

constexpr uint32_t kInvalidCaptureSlotIndex = std::numeric_limits<uint32_t>::max();
constexpr uint64_t kInvalidAcquisitionToken = 0;

struct Size
{
    uint32_t width;
    uint32_t height;
};

struct Rectangle
{
    uint32_t x;
    uint32_t y;
    Size size;
};

struct CaptureCapabilities
{
    uint32_t size{sizeof(CaptureCapabilities)};
    uint32_t drmFormat;
    uint64_t drmModifier;
    Size maximumContentSize;
    uint32_t minimumSlots;
    uint32_t maximumSlots;
};

enum class CaptureYcbcrMatrix : uint32_t
{
    UNKNOWN = 0,
    BT601 = 1,
    BT709 = 2,
    BT2020 = 3,
};

enum class CaptureSampleRange : uint32_t
{
    UNKNOWN = 0,
    FULL = 1,
    NARROW = 2,
};

enum class CaptureChromaSiting : uint32_t
{
    UNKNOWN = 0,
    COSITED = 1,
    MIDPOINT = 2,
};

/* Effective per-frame colour interpretation (section 6.2). Every field is set
 * for a successful YUV acquisition; UNKNOWN is valid only for an empty record
 * or for RGB fields that do not apply. */
struct CaptureColorDescription
{
    CaptureYcbcrMatrix matrix{CaptureYcbcrMatrix::UNKNOWN};
    CaptureSampleRange range{CaptureSampleRange::UNKNOWN};
    CaptureChromaSiting horizontalSiting{CaptureChromaSiting::UNKNOWN};
    CaptureChromaSiting verticalSiting{CaptureChromaSiting::UNKNOWN};
};

struct DmaBufObject
{
    uint64_t objectId;
    uint64_t sizeBytes;
};

struct FramePlane
{
    uint32_t planeIndex;
    uint64_t dmaBufObjectId;
    uint64_t offsetBytes;
    uint64_t lengthBytes;
    uint32_t strideBytes;
};

struct CaptureSlotLayout
{
    uint32_t slotIndex;
    std::vector<FramePlane> planes;
};

struct ExportedDmaBuf
{
    uint64_t dmaBufObjectId;
    int fd{-1};
};

struct CapturePool
{
    Size backingSize;
    std::vector<CaptureSlotLayout> slotLayouts;
};

struct CapturedFrame
{
    uint32_t slotIndex{kInvalidCaptureSlotIndex};
    uint64_t acquisitionToken{kInvalidAcquisitionToken};
    int64_t presentationTimeNs;
    Rectangle visibleRegion;
    bool dmaBufObjectsFlushed{false};
    CaptureCapabilities capabilities{};
    CaptureColorDescription colorDescription{};
    Size backingSize{};
    std::vector<DmaBufObject> dmaBufObjects;
    std::vector<FramePlane> planes;
    std::vector<ExportedDmaBuf> exportedDmaBufs;

    CapturedFrame() : presentationTimeNs{0}, visibleRegion{} {}
    CapturedFrame(const CapturedFrame &) = delete;
    CapturedFrame &operator=(const CapturedFrame &) = delete;
    CapturedFrame(CapturedFrame &&other) noexcept;
    CapturedFrame &operator=(CapturedFrame &&other) noexcept = delete;
    ~CapturedFrame() = default;

    /* True when no release is owed and no FD is held (section 5.1). */
    bool isEmpty() const
    {
        if (slotIndex != kInvalidCaptureSlotIndex || acquisitionToken != kInvalidAcquisitionToken)
            return false;
        for (const ExportedDmaBuf &e : exportedDmaBufs)
        {
            if (e.fd != -1)
                return false;
        }
        return true;
    }
};

/* Transfers metadata, FDs, token and release obligation without dup()/close();
 * the source is left empty with its flag cleared (section 5.1). */
inline CapturedFrame::CapturedFrame(CapturedFrame &&other) noexcept
    : slotIndex{other.slotIndex}, acquisitionToken{other.acquisitionToken},
      presentationTimeNs{other.presentationTimeNs}, visibleRegion{other.visibleRegion},
      dmaBufObjectsFlushed{other.dmaBufObjectsFlushed}, capabilities{other.capabilities},
      colorDescription{other.colorDescription}, backingSize{other.backingSize}, dmaBufObjects{std::move(other.dmaBufObjects)},
      planes{std::move(other.planes)}, exportedDmaBufs{std::move(other.exportedDmaBufs)}
{
    other.slotIndex = kInvalidCaptureSlotIndex;
    other.acquisitionToken = kInvalidAcquisitionToken;
    other.dmaBufObjectsFlushed = false;
    other.colorDescription = CaptureColorDescription{};
    other.dmaBufObjects.clear();
    other.planes.clear();
    other.exportedDmaBufs.clear();
}

class ICaptureSession
{
public:
    virtual ~ICaptureSession() = default;

    virtual CaptureStatus getCapabilities(CaptureCapabilities &out) const = 0;

    virtual CaptureStatus createPool(uint32_t slotCount, CapturePool &outPool) = 0;

    virtual CaptureStatus acquireCurrentFrame(CapturedFrame &outFrame) = 0;

    virtual CaptureStatus releaseFrame(CapturedFrame &frame) = 0;
};

inline const char *captureStatusName(CaptureStatus status)
{
    switch (status)
    {
    case CaptureStatus::OK:               return "OK";
    case CaptureStatus::NO_NEW_FRAME:     return "NO_NEW_FRAME";
    case CaptureStatus::UNSUPPORTED:      return "UNSUPPORTED";
    case CaptureStatus::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case CaptureStatus::NO_RESOURCES:     return "NO_RESOURCES";
    case CaptureStatus::NO_FRAME:         return "NO_FRAME";
    case CaptureStatus::FATAL_ERROR:      return "FATAL_ERROR";
    }
    return "UNKNOWN";
}

} // namespace rdkcapture

/* Vendor library factory (section 8.1). MW loads the library with dlopen(),
 * resolves this symbol and owns the returned session. MW destroys it with
 * delete through the virtual destructor (section 4.6), only after the pipeline
 * has reached NULL and every acquired frame has been released, and keeps the
 * library loaded until that deletion completes. */
#define RDK_CAPTURE_CREATE_SESSION_SYMBOL "createCaptureSession"

extern "C"
{
    rdkcapture::ICaptureSession *createCaptureSession();
}

using CreateCaptureSessionFn = rdkcapture::ICaptureSession *(*)();

#endif /* RDK_CAPTURE_ICAPTURESESSION_H */
