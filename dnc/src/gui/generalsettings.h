/*
 * Dynamic Network Directory Service
 * Copyright (C) 2009-2014
 * Nicolas J. Bouliane <nib@dynvpn.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 */

#ifndef GENERALSETTINGS_H
#define GENERALSETTINGS_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDialog>

#include "maindialog.h"
#include "ui_generalsettings.h"

class GeneralSettings: public QDialog
{
	Q_OBJECT

	public:
		GeneralSettings(MainDialog *dialog);
		virtual ~GeneralSettings();

	public slots:
		void slotCheckUpdate();
		void slotCheckAutoConnect();
		void slotDownloadFinished(QNetworkReply *reply);

	private:
		Ui::GeneralSettings ui;

		QNetworkAccessManager manager;
		QList<QNetworkReply *> currentDownloads;

		QString StableVersion;
		QString CurrentVersion;
		QMovie *movie;
};

#endif // GENERALSETTINGS_H
