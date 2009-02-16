#ifndef RESULTLOADWIDGET_H
#define RESULTLOADWIDGET_H

#include "FarsightConfig.h"

#include <iostream>
#include <fstream>

//QT INCLUDES
#include <QtGui/QWidget>
#include <QtGui/QFileDialog>
#include <QtCore/QString>

//GUI INCLUDES
#include "PlotWindow.h"
#include "TableWindow.h"
#include "SegmentationWindow.h"

//OTHER LOCAL INCLUDES
#include "FTKImage/ftkImage.h"
#include "tinyxml/tinyxml.h"


//***************************************************************************
// THIS CLASS LOADS DIRECTLY FROM AN XML FILE TO DISPLAY RESULTS
// AND ALLOW FOR RESULTS NAVIGATION AND VIEWING.
// EDITING IS NOT ALLOWED BECAUSE THERE IS NO CONNECTION TO THE 
// MODULE/PACKAGE THAT CREATED THESE RESULTS.  
//***************************************************************************
class ResultLoadWidget : public QWidget
{
    Q_OBJECT;

public:
    ResultLoadWidget(FTKItemModel*,QItemSelectionModel*);
	void CreateNewPlotWindow();

signals:
	void closing(QWidget *widget);

protected:
	void closeEvent(QCloseEvent *event);

private slots:
	void closeWidget(QWidget *widget);

private:
	SegmentationWindow *segWin;
	TableWindow *tblWin;
	std::vector<PlotWindow *> pltWin;

	FTKItemModel *model;				  //comes from parent
	QItemSelectionModel *selectionModel;  //comes from parent

	ftkImage *dataImg;
	ftkImage *labelImg;

	QString path;
	QString filename;
	QString datafname;
	QString labelfname;

	void loadFromResultImages();
	void loadFromXML();
	void loadXML();
	void loadOutliers();
	void extractPath();
	//void CreateNewPlotWindow();
	void CreateNewTableWindow();
	void CreateNewSegmentationWindow();
};


#endif