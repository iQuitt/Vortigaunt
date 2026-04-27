#include <QApplication>
#include <QDebug>
#include <QMessageBox>

#include <QString>
#include "AboutDialog.h"
#include <VortigauntVersion.h>


void AboutDialog(QWidget* parent)
{
	const QString AppName = "Vortigaunt Tool";

	QString locStr = QString::number(VORTIGAUNT_LINES_OF_CODE);

	QMessageBox::about(parent, "About Vortigaunt",
		
		QString::fromUtf8(
			reinterpret_cast<const char*>(u8R"(%1 %2

Vortigaunt - A Porting Tool for GoldSrc Engine

Copyright 2026 
iQuit

Email:	x@x.com


Git Info:
	Build Type: %3
	Branch: %4
	Commit: %5
	OS: %6

Project Stats:
	Lines of Code: %9

Using 3rd Party Libraries:
	Granny 3D SDK - 1999-2017 by RAD Game Tools, Inc.
	assimp - Copyright 2006-2026, assimp team  
	dr_wav - Copyright 2017-2026, David Reid 
	dr_mp3 - Copyright 2017-2026, David Reid 
	Discord Social SDK - Copyright 2015-2026, Discord Inc. 
	libimagequant © 2009-2018 by Kornel Lesiński. 
	Lithtech SDK - Copyright 1998-2005, Monolith Productions, Inc. 
	Lzma SDK - Copyright 1999-2026, Igor Pavlov 

Special Thanks To:
	Luís Leite for Counter Strike Online PAK (https://git.sr.ht/~leite/cso-pak)
	Kungfulon for Crossfire REZ  (https://gist.github.com/kungfulon/dfa49323eb7a55db964f10174e57c19f)
	Luigi Auriemma for XFS logic (https://aluigi.altervista.org/bms/xenesis.bms)
	YoungFine0825 for LTB2FBX and DTX to TGA (https://github.com/YoungFine0825/LTB2FBX)
	

Uses Qt: %7

Build Date: %8 [%10]
)"))
	.arg(AppName)
	.arg(VORTIGAUNT_VERSION_STRING)
	.arg(VORTIGAUNT_BUILD_TYPE)
	.arg(VORTIGAUNT_GIT_BRANCH)
	.arg(VORTIGAUNT_GIT_COMMIT)
	.arg(VORTIGAUNT_OS)
	.arg(QT_VERSION_STR)
	.arg(__DATE__)
	.arg(locStr)
	.arg(__TIME__)

);
}

