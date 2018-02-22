/*ckwg +29
 * Copyright 2016 by Kitware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name Kitware, Inc. nor the names of any contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SaveKeyFrameTool.h"
#include "ConfigHelper.h"

#include <iomanip>

#include <vital/algo/image_io.h>
#include <vital/algo/video_input.h>
#include <kwiversys/SystemTools.hxx>

#include <QtCore/QDebug>
#include <QtGui/QMessageBox>

using kwiver::vital::algo::video_input;
using kwiver::vital::algo::video_input_sptr;
using kwiver::vital::algo::image_io;
using kwiver::vital::algo::image_io_sptr;

namespace
{
static char const* const BLOCK_VR = "video_reader";
static char const* const BLOCK_IW = "image_writer";
}

QTE_IMPLEMENT_D_FUNC(SaveKeyFrameTool)

//-----------------------------------------------------------------------------
class SaveKeyFrameToolPrivate
{
public:
  video_input_sptr video_reader;
  image_io_sptr image_writer;
};

//-----------------------------------------------------------------------------
SaveKeyFrameTool::SaveKeyFrameTool(QObject* parent)
  : AbstractTool(parent), d_ptr(new SaveKeyFrameToolPrivate)
{
  this->setText("Save Key Frames");
  this->setToolTip(
    "Saves the key frames from the Track Features to disk.");
}

//-----------------------------------------------------------------------------
SaveKeyFrameTool::~SaveKeyFrameTool()
{
}

//-----------------------------------------------------------------------------
AbstractTool::Outputs SaveKeyFrameTool::outputs() const
{
  return KeyFrames;
}

//-----------------------------------------------------------------------------
bool SaveKeyFrameTool::execute(QWidget* window)
{
  QTE_D();

  if (!this->hasTracks() || !this->hasVideoSource())
  {
    QMessageBox::information(
      window, "Insufficient data",
      "This operation requires tracks and a video source.");
    return false;
  }

  if (!video_input::check_nested_algo_configuration(
    BLOCK_VR, this->data()->config))
  {
    QMessageBox::critical(
      window, "Configuration error",
      "An error was found in the video source algorithm configuration.");
    return false;
  }

  // Merge project config with default config file
  auto const config = ConfigHelper::readConfig("gui_keyframe_image_writer.conf");

  // Check configuration
  if (!config)
  {
    QMessageBox::critical(
      window, "Configuration error",
      "No configuration data was found. Please check your installation.");
    return false;
  }

  config->merge_config(this->data()->config);
  if (!image_io::check_nested_algo_configuration(BLOCK_IW, config))
  {
    QMessageBox::critical(
      window, "Configuration error",
      "An error was found in the image writer algorithm configuration.");
    return false;
  }

  video_input::set_nested_algo_configuration(
    BLOCK_VR, this->data()->config, d->video_reader);
  image_io::set_nested_algo_configuration(
    BLOCK_IW, config, d->image_writer);

  return AbstractTool::execute(window);
}

//-----------------------------------------------------------------------------
void SaveKeyFrameTool::run()
{
  QTE_D();

  // Create keyframes directory if it doesn't exist
  if (!kwiversys::SystemTools::FileExists("results/keyframes"))
  {
    kwiversys::SystemTools::MakeDirectory("results/keyframes");
  }

  kwiver::vital::timestamp currentTimestamp;

  d->video_reader->open(this->data()->videoPath);

  for (auto const& frame: this->tracks()->keyframes())
  {
    if (d->video_reader->seek_frame(currentTimestamp, frame))
    {
      std::ostringstream ss;
      ss << "results/keyframes/frame_" << std::setw(4) << std::setfill('0') << frame << ".png";
      auto filename = ss.str();
      try
      {
        auto frameImg = d->video_reader->frame_image();
        d->image_writer->save(ss.str(), d->video_reader->frame_image());
      }
      catch (std::exception const& e)
      {
        qWarning() << "Error writing frame to "
        << QString::fromStdString(filename) << ": " <<  e.what();
      }
    }
    else
    {
      qWarning() << "Key frame " << frame << " not available in video source.";
    }
  }

  d->video_reader->close();
}
