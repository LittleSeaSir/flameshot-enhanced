// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "modificationcommand.h"
#include "capturewidget.h"

AppendPencilCommand::AppendPencilCommand(CaptureWidget* captureWidget,
                                         CaptureTool* pencil)
  : m_captureWidget(captureWidget)
  , m_pencil(pencil->copy())
{}

void AppendPencilCommand::undo()
{
    m_captureWidget->removeLastPencil();
}

void AppendPencilCommand::redo()
{
    m_captureWidget->appendPencil(m_pencil.get());
}

ModificationCommand::ModificationCommand(
  CaptureWidget* captureWidget,
  const CaptureToolObjects& captureToolObjects,
  const CaptureToolObjects& captureToolObjectsBackup)
  : m_captureWidget(captureWidget)
{
    m_captureToolObjects = captureToolObjects;
    m_captureToolObjectsBackup = captureToolObjectsBackup;
}

void ModificationCommand::undo()
{
    m_captureWidget->setCaptureToolObjects(m_captureToolObjectsBackup);
}

void ModificationCommand::redo()
{
    m_captureWidget->setCaptureToolObjects(m_captureToolObjects);
}
