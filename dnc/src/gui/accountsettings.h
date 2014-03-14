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

#ifndef ACCOUNTSETTINGS_H
#define ACCOUNTSETTINGS_H

#include <QDialog>
#include <QString>

#include "maindialog.h"
#include "ui_accountsettings.h"

class AccountSettings: public QDialog
{
	Q_OBJECT

	public:
		AccountSettings(MainDialog *dialog);
		virtual ~AccountSettings();

	public slots:
		void slotOnConnect(QString ip);
		void slotConnWaiting();

	private:
		Ui::AccountSettings ui;
		QMovie *movie;
};

#endif // ACCOUNTSETTINGS_H
