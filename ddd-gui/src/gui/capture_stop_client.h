/************************************************************************

    capture_stop_client.h

    --stop-capture, the other end of the control socket
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

class QTextStream;

namespace ddd::gui {

struct StopCaptureOptions {
  // Empty means the socket every instance listens on. The tests name their own,
  // so that one running here cannot reach an application running on the bench.
  QString server_name;

  int connect_timeout_milliseconds = 3000;

  // Generous, and it is not a guess at how long stopping takes. Stopping is the
  // encoder finishing a file: on a slow disk, at the end of a disc side, that
  // is as long as it is, and a client that gave up early would report a failure
  // for a capture that was about to be written perfectly well.
  int reply_timeout_milliseconds = 60000;
};

// Ask the running application to stop its capture, and wait until the file is
// finished.
//
// The waiting is the point. A script that stops a capture and immediately reads
// the file needs the file to be closed, the duration rename to have happened
// and the sidecar to be written — none of which is true when the capture stops,
// only when it has finished. This returns after all of it.
//
// The path of the finished capture goes to stdout, alone and unadorned, so that
// a script can take it as it stands; everything meant for a person reading
// along goes to stderr. Returns a CaptureCliExit code: success, nothing running
// to stop, or a stop that did not produce a file.
//
// Driven by an event loop rather than QLocalSocket's blocking waits, so that a
// test can host the server on the same thread as the client and have the two
// talk to each other.
int RunStopCapture(QTextStream& out, QTextStream& error,
                   const StopCaptureOptions& options = {});

}  // namespace ddd::gui
