/*  This file is part of the KDE project

    Copyright (C) 2007 John Tapsell <tapsell@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "processes_remote_p.h"
#include "process.h"

#include <QString>
#include <QSet>
#include <QTimer>

#include <klocale.h>
#include <kdebug.h>





namespace KSysGuard
{

  class ProcessesRemote::Private
  {
    public:
      Private() {havePsInfo = false; pidColumn = 1; 
	      ppidColumn= ppidColumn = nameColumn = uidColumn = gidColumn = 
	      statusColumn = userColumn = systemColumn = niceColumn = 
	      vmSizeColumn = vmRSSColumn = loginColumn = commandColumn = -1;
              usedMemory = freeMemory;}
      ~Private() {;}
      QString host;
      QList<QByteArray> lastAnswer;
      QSet<long> pids;
      QHash<long, QList<QByteArray> > processByPid;

      bool havePsInfo;
      int pidColumn;
      int ppidColumn;
      int nameColumn;
      int uidColumn;
      int gidColumn;
      int statusColumn;
      int userColumn;
      int systemColumn;
      int niceColumn;
      int vmSizeColumn;
      int vmRSSColumn;
      int loginColumn;
      int commandColumn;

      int numColumns;

      long freeMemory;
      long usedMemory;

    };
ProcessesRemote::ProcessesRemote(const QString &hostname) : d(new Private())
{
  d->host = hostname;
  QTimer::singleShot(0, this, SLOT(setup()));
}

void ProcessesRemote::setup() {
  emit runCommand("mem/physical/used", (int)UsedMemory);
  emit runCommand("mem/physical/free", (int)FreeMemory);
  emit runCommand("ps?", (int)PsInfo);
  emit runCommand("ps", (int)Ps);
}


long ProcessesRemote::getParentPid(long pid) {
    if(!d->processByPid.contains(pid)) {
        kDebug() << "Parent pid requested for pid that we do not have info on " << pid;
        return 0;
    }
    if(d->ppidColumn == -1) {
        kDebug() << "ppid column not known ";
        return 0;
    }
    return d->processByPid[pid].at(d->ppidColumn).toLong();
}
bool ProcessesRemote::updateProcessInfo( long pid, Process *process)
{
    Q_CHECK_PTR(process);
    if(!d->processByPid.contains(pid)) {
	kDebug() << "update request for pid that we do not have info on " << pid;
        return false;
    }
    QList<QByteArray> p = d->processByPid[pid];

    if(d->nameColumn!= -1) process->name = p.at(d->nameColumn);
    if(d->uidColumn!= -1) process->uid = p.at(d->uidColumn).toLong();
    if(d->gidColumn!= -1) process->gid = p.at(d->gidColumn).toLong();
    if(d->statusColumn!= -1) {
	    switch( p.at(d->statusColumn)[0] ) {
		    case 's':
			    process->status = Process::Sleeping;
			    break;
		    case 'r':
			    process->status = Process::Running;
			    break;
	    }
    }
    if(d->userColumn!= -1) process->userUsage = p.at(d->userColumn).toLong();
    if(d->systemColumn!= -1) process->sysUsage = p.at(d->systemColumn).toLong();
    if(d->niceColumn!= -1) process->niceLevel = p.at(d->niceColumn).toLong();
    if(d->vmSizeColumn!= -1) process->vmSize = p.at(d->vmSizeColumn).toLong();
    if(d->vmRSSColumn!= -1) process->vmRSS = p.at(d->vmRSSColumn).toLong();
    if(d->loginColumn!= -1) process->login = QString::fromUtf8(p.at(d->loginColumn).data());
    if(d->commandColumn!= -1) process->command = QString::fromUtf8(p.at(d->commandColumn).data());

    return true;
}

QSet<long> ProcessesRemote::getAllPids( )
{
    if(!d->havePsInfo)
    	emit runCommand("ps?", (int)PsInfo);
    emit runCommand("ps", (int)Ps);
    d->pids.clear();
    d->processByPid.clear();
    foreach(QByteArray process, d->lastAnswer) {
        QList<QByteArray> info = process.split('\t');
	if(info.size() == d->numColumns) {
		int pid =  info.at(d->pidColumn).toLong();
		Q_ASSERT(! d->pids.contains(pid));
		d->pids << pid;
		d->processByPid[pid] = info;
	}
    }
    return d->pids;
}

bool ProcessesRemote::sendSignal(long pid, int sig) {
	//TODO run the proper command for all these functions below
    //emit runCommand("kill ", (int)Ps);
    return true;
}
bool ProcessesRemote::setNiceness(long pid, int priority) {
    return true;
}

bool ProcessesRemote::setIoNiceness(long pid, int priorityClass, int priority) {
    return false; //Not yet supported
}

bool ProcessesRemote::setScheduler(long pid, int priorityClass, int priority) {
    return false;
}

bool ProcessesRemote::supportsIoNiceness() {
    return false;
}

long long ProcessesRemote::totalPhysicalMemory() {
    return d->usedMemory + d->freeMemory;
}
long ProcessesRemote::numberProcessorCores() {
    return 0;
}

void ProcessesRemote::answerReceived( int id, const QList<QByteArray>& answer ) {
    kDebug() << "Answer received in remote ";
    switch (id) {
        case PsInfo: {
            kDebug() << "psinfo";
            if(answer.isEmpty()) return; //Invalid data
            QList<QByteArray> info = answer.at(0).split('\t');
	    d->numColumns = info.size();
	    for(int i =0; i < d->numColumns; i++) {
                if(info[i] == "Name")
			d->nameColumn = i;
		else if(info[i] == "PID")
			d->pidColumn = i;
		else if(info[i] == "PPID")
			d->ppidColumn = i;
		else if(info[i] == "UID")
			d->uidColumn = i;
		else if(info[i] == "GID")
			d->gidColumn = i;
		else if(info[i] == "Status")
			d->statusColumn = i;
		else if(info[i] == "User%")
			d->userColumn = i;
		else if(info[i] == "System%")
			d->systemColumn = i;
		else if(info[i] == "Nice")
			d->niceColumn = i;
		else if(info[i] == "VmSize")
			d->vmSizeColumn = i;
		else if(info[i] == "VmRSS")
			d->vmRSSColumn = i;
		else if(info[i] == "Login")
			d->loginColumn = i;
		else if(info[i] == "Command")
			d->commandColumn = i;
	    }
	    d->havePsInfo = true;
	    break;
	}
        case Ps:
	    d->lastAnswer = answer;
            kDebug() << "ps";
	    if(!d->havePsInfo) return;  //Not setup yet.  Should never happen
	case FreeMemory:
            if(answer.isEmpty()) return; //Invalid data
	    d->freeMemory = answer[0].toLong();
	    break;
	case UsedMemory:
            if(answer.isEmpty()) return; //Invalid data
	    d->usedMemory = answer[0].toLong();
	    break;
    }

}

ProcessesRemote::~ProcessesRemote()
{
    delete d;
}

}

#include "processes_remote_p.moc"

