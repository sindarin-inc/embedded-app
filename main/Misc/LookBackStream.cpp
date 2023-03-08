#include "LookBackStream.hpp"

#include "sindarin-debug.h"

auto LookBackStream::available() -> int { return size() - position(); }

auto LookBackStream::readBytes(uint8_t *buf, int size) -> int {
    int bytesCopied = 0;
    while (bytesCopied < size) {
        // first check if we are reading from the lookback buffer
        if (lookBackBytes_ > 0) {
            int bytesToCopy = std::min((int)size - bytesCopied, lookBackBytes_);

            if (buf) {
                lookBackBuffer_.copyFrom(lookBackBuffer_.size() - lookBackBytes_, &buf[bytesCopied],
                                         bytesToCopy);
            }

            lookBackBytes_ -= bytesToCopy;
            bytesCopied += bytesToCopy;
            position_ += bytesToCopy;

            continue;
        }

        // no bytes in the lookback buffer, but still reading
        // now read directly from the input stream
        int bytesToCopy = (int)size - bytesCopied;
        int bytesRead = stream_->readBytes(&buf[bytesCopied], bytesToCopy);

        // push result into lookback buffer
        lookBackBuffer_.push(&buf[bytesCopied], bytesRead);

        bytesCopied += bytesRead;
        position_ += bytesRead;

        if (bytesRead < bytesToCopy) {
            // not a full read, must be the end of the stream
            return bytesCopied;
        }
    }

    return size;
}

auto LookBackStream::read() -> int {
    uint8_t c;
    int bytesRead = readBytes(&c, 1);
    if (bytesRead != 1) {
        return -1;
    }

    return c;
}

auto LookBackStream::seek(uint32_t pos) -> bool {
    // check direction of seek
    if (pos == position_) {
        // nothing to do
        return true;
    }

    int bytesToReadForward = 0;
    if (pos > position_) {
        int offset = pos - position_;
        if (offset <= lookBackBytes_) {
            // enough bytes in lookback buffer to just skip forward
            lookBackBytes_ -= offset;
            position_ += offset;
            return true;
        }

        // skip forward to end of lookBackBuffer
        offset -= lookBackBytes_;

        // this skips to the end of the lookback buffer
        readBytes(nullptr, lookBackBytes_);

        // check how far we need to seek
        if (offset > lookBackBuffer_.capacity()) {
            // clear buffer
            lookBackBuffer_.clear();
            lookBackBytes_ = 0;
            // Seek forward so we fill the entire size of the look back buffer immediately before
            // the desired position
            stream_->seek(pos - lookBackBuffer_.capacity());
            position_ = stream_->position();
            offset = lookBackBuffer_.capacity();
        }

        // now read bytes until the position is reached
        // this copies data into the lookback buffer
        bytesToReadForward = offset;
    } else {
        // seeking backwards
        int offset = position_ - pos;
        if (lookBackBytes_ + offset <= lookBackBuffer_.size()) {
            // we have enough bytes to seek back within the lookback buffer
            lookBackBytes_ += offset;
            position_ = pos;
            return true;
        } else {
            // need to seek the input stream backwards
            // clear the lookback buffer
            lookBackBuffer_.clear();
            lookBackBytes_ = 0;

            // try to seek the full capacity of the buffer before the target position,
            // so the lookback buffer can fill up again.
            int seekPos = std::max((int)pos - lookBackBuffer_.capacity(), 0);
            stream_->seek(seekPos);
            position_ = stream_->position();

            // need to read some data to fill up the lookback buffer
            bytesToReadForward = pos - position_;
        }
    }

    if (bytesToReadForward > 0) {
        // need to read forward to fill up the lookback buffer
        uint8_t buf[1024];
        int bytesCopied = 0;
        while (bytesCopied < bytesToReadForward) {
            int bytesToCopy = std::min(bytesToReadForward - bytesCopied, (int)sizeof(buf));
            int bytesRead = readBytes(buf, bytesToCopy);
            bytesCopied += bytesRead;
            if (bytesRead != bytesToCopy) {
                log_i("failed to seek to %d", pos);
                return false;
            }
        }
    }

    return true;
}

auto LookBackStream::position() -> size_t { return position_; }

auto LookBackStream::size() -> size_t { return stream_->size(); }