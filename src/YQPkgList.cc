/**************************************************************************
Copyright (C) 2000 - 2010 Novell, Inc.
All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

**************************************************************************/


/*---------------------------------------------------------------------\
|								       |
|		       __   __	  ____ _____ ____		       |
|		       \ \ / /_ _/ ___|_   _|___ \		       |
|			\ V / _` \___ \ | |   __) |		       |
|			 | | (_| |___) || |  / __/		       |
|			 |_|\__,_|____/ |_| |_____|		       |
|								       |
|				core system			       |
|							 (C) SuSE GmbH |
\----------------------------------------------------------------------/

  File:	      YQPkgList.cc

  Author:     Stefan Hundhammer <sh@suse.de>

  Textdomain "qt-pkg"

/-*/



#include <algorithm>
using std::max;

#define YUILogComponent "qt-pkg"
#include "YUILog.h"
#include <QPixmap>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QFile>
#include <QTreeView>
#include <QDebug>
#include <QHeaderView>
#include <QFontMetrics>

#include "utf8.h"

#include "YQPkgList.h"
#include "YQUI.h"
#include "YQi18n.h"
#include "YQIconPool.h"
#include "YQApplication.h"

#define SINGLE_VERSION_COL	1


YQPkgList::YQPkgList( QWidget * parent )
    : YQPkgObjList( parent )
{
    _srpmStatusCol	= -42;

    resetOptimalColumnWidthValues();

    int numCol = 0;
    QStringList headers;
    QString 	versionHeaderText;
    QString 	instVersionHeaderText;


    headers <<  "";			_statusCol	= numCol++;
    headers <<  _( "Package" 	);	_nameCol	= numCol++;
    headers <<  _( "Summary" 	);	_summaryCol	= numCol++;

    if ( haveInstalledPkgs() )
    {
#if SINGLE_VERSION_COL
        versionHeaderText = _("Installed (Available)");
        headers << versionHeaderText;	_instVersionCol = numCol++;
	_versionCol = _instVersionCol;
#else
	versionHeaderText = _( "Avail. Ver." );
	headers << versionHeaderText;	_versionCol	= numCol++;

	instVersionHeaderText = _( "Inst. Ver."  );
	headers << instVersionHeaderText;  _instVersionCol = numCol++;
#endif
    }
    else
    {
	versionHeaderText = _( "Version" );
	headers << versionHeaderText;	_versionCol	= numCol++;
    }

    // headers <<  _( "Source" );	_srpmStatusCol	= numCol++;
    headers <<  _( "Size" 	);	_sizeCol	= numCol++;

    setHeaderLabels( headers );

    header()->setSortIndicatorShown( true );
    header()->setSectionsClickable( true );

    sortByColumn( statusCol(), Qt::AscendingOrder );
    setAllColumnsShowFocus( true );
    setIconSize( QSize( 22, 16 ) );

    header()->setSectionResizeMode( QHeaderView::Interactive );

    /* NOTE: resizeEvent() is automatically triggered afterwards => sets initial column widths */

    createActions();
    createSourceRpmContextMenu();

    connect ( header(), SIGNAL( sectionClicked (int) ),
	      this,	SLOT( sortByColumn (int) ) );

}


YQPkgList::~YQPkgList()
{
    // NOP
}


void YQPkgList::addPkgItem( ZyppSel selectable,
			    ZyppPkg zyppPkg )
{
    addPkgItem( selectable, zyppPkg, false );
}


void YQPkgList::addPkgItemDimmed( ZyppSel selectable,
				  ZyppPkg zyppPkg )
{
    addPkgItem( selectable, zyppPkg, true );
}


void
YQPkgList::addPkgItem( ZyppSel	selectable,
		       ZyppPkg 	zyppPkg,
		       bool 	dimmed )
{
    scrollToTop();
    scheduleDelayedItemsLayout();

    if ( ! selectable )
    {
	yuiError() << "NULL zypp::ui::Selectable!" << std::endl;
	return;
    }

    YQPkgListItem * item = new YQPkgListItem( this, selectable, zyppPkg );
    Q_CHECK_PTR( item );

    updateOptimalColumnWidthValues(selectable, zyppPkg);
    optimizeColumnWidths();

    item->setDimmed( dimmed );
    applyExcludeRules( item );
}


bool
YQPkgList::haveInstalledPkgs()
{
    for ( ZyppPoolIterator it = zyppPkgBegin();
	  it != zyppPkgEnd();
	  ++it )
    {
	if ( (*it)->installedObj() )
	    return true;
    }

    return false;
}


void
YQPkgList::pkgObjClicked( int			button,
			  QTreeWidgetItem *	listViewItem,
			  int			col,
			  const QPoint &	pos )
{
    if ( col == srpmStatusCol() )
    {
	YQPkgListItem * item = dynamic_cast<YQPkgListItem *> (listViewItem);

	if ( item )
	{
	    if ( button == Qt::LeftButton )
	    {
		if ( editable() && item->editable() )
		    item->toggleSourceRpmStatus();
		return;
	    }
	    else if ( button == Qt::RightButton )
	    {
		if ( editable() && item->editable() )
		{
		    updateActions( item );

		    if ( _sourceRpmContextMenu )
			_sourceRpmContextMenu->popup( pos );
		}

		return;
	    }
	}
    }

    YQPkgObjList::pkgObjClicked( button, listViewItem, col, pos );
}


QSize
YQPkgList::sizeHint() const
{
    return QSize( 600, 350 );
}


void
YQPkgList::createSourceRpmContextMenu()
{
    _sourceRpmContextMenu = new QMenu( this );

    _sourceRpmContextMenu->addAction(actionInstallSourceRpm);
    _sourceRpmContextMenu->addAction(actionDontInstallSourceRpm);

    QMenu * submenu = new QMenu( _sourceRpmContextMenu );
    Q_CHECK_PTR( submenu );
    QAction *action = _sourceRpmContextMenu->addMenu( submenu );
    action->setText(_( "&All in This List" ));

    submenu->addAction(actionInstallListSourceRpms);
    submenu->addAction(actionDontInstallListSourceRpms);
}


void
YQPkgList::setInstallCurrentSourceRpm( bool installSourceRpm,
				       bool selectNextItem )
{
#if FIXME
    QTreeWidgetItem * listViewItem = selectedItem();

    if ( ! listViewItem )
	return;

    YQPkgListItem * item = dynamic_cast<YQPkgListItem *> (listViewItem);

    if ( item )
    {
	item->setInstallSourceRpm( installSourceRpm );

	if ( selectNextItem && item->nextSibling() )
	{
	    item->setSelected( false );			// doesn't emit signals
	    setSelected( item->nextSibling(), true );	// emits signals
	}
    }
#endif
}


void
YQPkgList::setInstallListSourceRpms( bool installSourceRpm )
{
    if ( ! _editable )
	return;

#if FIXME
    QTreeWidgetItem * listViewItem = firstChild();

    while ( listViewItem )
    {
	YQPkgListItem * item = dynamic_cast<YQPkgListItem *> (listViewItem);

	if ( item && item->editable() )
	{
	    item->setInstallSourceRpm( installSourceRpm );
	}

	listViewItem = listViewItem->nextSibling();
    }
#endif
}


void
YQPkgList::resetOptimalColumnWidthValues()
{
    _optimalColWidth_statusIcon = 0;
    _optimalColWidth_name = 0;
    _optimalColWidth_summary = 0;
    _optimalColWidth_version = 0;
    _optimalColWidth_instVersion = 0;
    _optimalColWidth_size = 0;
}


void
YQPkgList::updateOptimalColumnWidthValues(ZyppSel selectable, ZyppPkg zyppPkg)
{
    QString qstr;
    int qstr_width = 0;
    QFontMetrics fm( this->font() );
    const ZyppObj candidate = selectable->candidateObj();
    const ZyppObj installed = selectable->installedObj();
    // Status icon:
    _optimalColWidth_statusIcon = 28;
    // Name:
    qstr = QString::fromUtf8( zyppPkg->name().c_str() );
    qstr_width = fm.boundingRect( qstr ).width() + 8;
    if (qstr_width > _optimalColWidth_name)
        _optimalColWidth_name = qstr_width;
    // Summary:
    qstr = QString::fromUtf8( zyppPkg->summary().c_str() );
    qstr_width = fm.boundingRect( qstr ).width() + 8;
    if (qstr_width > _optimalColWidth_summary)
        _optimalColWidth_summary = qstr_width;
    // Version(s):
    if ( instVersionCol() == versionCol() )	// combined column, version string
    {
        if (installed)
            qstr = QString::fromUtf8( installed->edition().c_str() );
	else
	    qstr.clear();
        if (candidate && (!installed || (candidate->edition() != installed->edition())))
        {
            if (installed)
                qstr += " ";
            qstr += "(" + QString::fromUtf8( candidate->edition().c_str() ) + ")";
        }
        qstr_width = fm.boundingRect( qstr ).width() + 8;
        if (qstr_width > _optimalColWidth_version)
            _optimalColWidth_version = qstr_width;
    }
    else	// separate columns, version strings
    {
        if (candidate)
        {
            qstr = QString::fromUtf8( candidate->edition().c_str() );
            qstr_width = fm.boundingRect( qstr ).width() + 8;
            if (qstr_width > _optimalColWidth_version)
                _optimalColWidth_version = qstr_width;
        }
        if (installed)
        {
            qstr = QString::fromUtf8( installed->edition().c_str() );
            qstr_width = fm.boundingRect( qstr ).width() + 8;
            if (qstr_width > _optimalColWidth_instVersion)
                _optimalColWidth_instVersion = qstr_width;
        }
    }
    // Size:
    qstr = QString::fromUtf8( zyppPkg->installSize().asString().c_str() );
    qstr_width = fm.boundingRect( qstr ).width() + 8;
    if (qstr_width > _optimalColWidth_size)
        _optimalColWidth_size = qstr_width;    
}


void
YQPkgList::optimizeColumnWidths()
{
    int visibleSpace = 0;
    int optimalWidthsSum = 0;    // Sum of all optimal (sized-to-content-) column width values
    int numOptCol = 4;       // Nr. of columns for distribution of remaining space
    int statusIconColWidth = _optimalColWidth_statusIcon;

    if (statusIconColWidth == 0)
        statusIconColWidth = 28;
    optimalWidthsSum = _optimalColWidth_statusIcon + _optimalColWidth_name + _optimalColWidth_summary + _optimalColWidth_version + _optimalColWidth_size;
    if ( instVersionCol() != versionCol() )
    {
        optimalWidthsSum += _optimalColWidth_instVersion;
        numOptCol++;
    }
    // CHECK IF WE HAVE LESS VISIBLE SPACE THAN WE NEED:
    visibleSpace = this->viewport()->width();
    if (visibleSpace < 0) return;
    if (optimalWidthsSum >= visibleSpace)	// THERE IS NOT ENOUGH VISIBLE SPACE TO SHOW ALL CLOUMNS WITH OPTIMAL WIDTHS
    {
        /* ONLY REDUCE WIDTH OF THE "summary"-COLUMN BEYOND OPTIMAL WIDTH
	 * IF THIS IS NOT ENOUGH, WE WILL GET A HORIZONTAL SCROLL BAR		*/
        int reducedSummaryWidth = visibleSpace - optimalWidthsSum + _optimalColWidth_summary;
        if (reducedSummaryWidth < 100)
            reducedSummaryWidth = 100;
	// Set new column widths:
	this->setColumnWidth( statusCol(), statusIconColWidth );
	this->setColumnWidth( nameCol(), _optimalColWidth_name );
        this->setColumnWidth( summaryCol(), reducedSummaryWidth);
	this->setColumnWidth( versionCol(), _optimalColWidth_version );
	if ( instVersionCol() != versionCol() )
	    this->setColumnWidth( instVersionCol(), _optimalColWidth_instVersion );
	this->setColumnWidth( sizeCol(), _optimalColWidth_size);
    }
    else	// THERE IS ENOUGH VISIBLE SPACE
    {
        // DISTRIBUTE REMAINING VISIBLE SPACE TO ALL COLUMNS (except the satusIcon-column):
        // Calculate additional column widths:
        int addSpace = (visibleSpace - optimalWidthsSum) / numOptCol;
        int addSpaceR = (visibleSpace - optimalWidthsSum) % numOptCol;
        // Set new column widths:
        this->setColumnWidth( statusCol(), statusIconColWidth );
        this->setColumnWidth( nameCol(), _optimalColWidth_name + addSpace );
        this->setColumnWidth( summaryCol(), _optimalColWidth_summary + addSpace );
        this->setColumnWidth( versionCol(), _optimalColWidth_version + addSpace );
        if ( instVersionCol() != versionCol() )
            this->setColumnWidth( instVersionCol(), _optimalColWidth_instVersion + addSpace );
        this->setColumnWidth( sizeCol(), _optimalColWidth_size + addSpace + addSpaceR );
    }
}


void
YQPkgList::clear()
{
    YQPkgObjList::clear();
    resetOptimalColumnWidthValues();
    optimizeColumnWidths();
}


void
YQPkgList::resort()
{
    int col = sortColumn();
    Qt::SortOrder order = header()->sortIndicatorOrder();
    sortByColumn(col, order);
}


void
YQPkgList::resizeEvent(QResizeEvent *event)
{
    if (event->size().width() != event->oldSize().width())
        optimizeColumnWidths();
    /* NOTE: avoids column width optimization when the size changes
       because the horizontal scroll bar appeares/disappeares */
    event->accept();
}


void
YQPkgList::createNotInstalledContextMenu()
{
    _notInstalledContextMenu = new QMenu( this );
    Q_CHECK_PTR( _notInstalledContextMenu );

    _notInstalledContextMenu->addAction(actionSetCurrentInstall);
    _notInstalledContextMenu->addAction(actionSetCurrentDontInstall);
    _notInstalledContextMenu->addAction(actionSetCurrentTaboo);
    _notInstalledContextMenu->addAction(actionShowCurrentSolverInfo);        

    addAllInListSubMenu( _notInstalledContextMenu );

    _notInstalledContextMenu->addSeparator();
    _notInstalledContextMenu->addAction( _( "Export This List to &Text File..." ),
					  this, SLOT( askExportList() ) );
}


void
YQPkgList::createInstalledContextMenu()
{
    _installedContextMenu = new QMenu( this );
    Q_CHECK_PTR( _installedContextMenu );

    _installedContextMenu->addAction(actionSetCurrentKeepInstalled);
    _installedContextMenu->addAction(actionSetCurrentDelete);
    _installedContextMenu->addAction(actionSetCurrentUpdate);
    _installedContextMenu->addAction(actionSetCurrentUpdateForce);
    _installedContextMenu->addAction(actionSetCurrentProtected);
    _installedContextMenu->addAction(actionShowCurrentSolverInfo);            

    addAllInListSubMenu( _installedContextMenu );

    _installedContextMenu->addSeparator();
    _installedContextMenu->addAction( _( "Export This List to &Text File..." ),
				       this, SLOT( askExportList() ) );
}


QMenu *
YQPkgList::addAllInListSubMenu( QMenu * menu )
{
    QMenu * submenu = new QMenu( menu );
    Q_CHECK_PTR( submenu );

    submenu->addAction(actionSetListInstall);
    submenu->addAction(actionSetListDontInstall);
    submenu->addAction(actionSetListKeepInstalled);
    submenu->addAction(actionSetListDelete);
    submenu->addAction(actionSetListDelete);
    submenu->addAction(actionSetListUpdate);
    submenu->addAction(actionSetListUpdateForce);
    submenu->addAction(actionSetListTaboo);
    submenu->addAction(actionSetListProtected);

    QAction *action = menu->addMenu( submenu );
    action->setText(_( "&All in This List" ));

    return submenu;
}


void
YQPkgList::createActions()
{
    actionInstallSourceRpm		= createAction( _( "&Install Source" ),
							statusIcon( S_Install, true ),
							statusIcon( S_Install, false ) );

    actionDontInstallSourceRpm		= createAction( _( "Do &Not Install Source" ),
							statusIcon( S_NoInst, true ),
							statusIcon( S_NoInst, false ) );

    actionInstallListSourceRpms		= createAction( _( "&Install All Available Sources" ),
							statusIcon( S_Install, true ),
							statusIcon( S_Install, false ),
							QString::null,		// key
							true );			// enabled

    actionDontInstallListSourceRpms	= createAction( _( "Do &Not Install Any Sources" ),
							statusIcon( S_NoInst, true ),
							statusIcon( S_NoInst, false ),
							QString::null,		// key
							true );			// enabled

    connect( actionInstallSourceRpm,          &QAction::triggered,
             this,                            static_cast<void (YQPkgList::*)()>(&YQPkgList::setInstallCurrentSourceRpm) );
    connect( actionDontInstallSourceRpm,      &QAction::triggered,
             this,                            &YQPkgList::setDontInstallCurrentSourceRpm );
    connect( actionInstallListSourceRpms,     &QAction::triggered,
             this,                            static_cast<void (YQPkgList::*)()>(&YQPkgList::setInstallListSourceRpms) );
    connect( actionDontInstallListSourceRpms, &QAction::triggered,
             this,                            &YQPkgList::setDontInstallListSourceRpms );
}


void
YQPkgList::updateActions( YQPkgObjListItem * pkgObjListItem )
{
    YQPkgObjList::updateActions( pkgObjListItem );

    YQPkgListItem * item = dynamic_cast<YQPkgListItem *> (pkgObjListItem);

    if ( item )
    {
	actionInstallSourceRpm->setEnabled( item->hasSourceRpm() );
	actionDontInstallSourceRpm->setEnabled( item->hasSourceRpm() );
    }
    else
    {
	actionInstallSourceRpm->setEnabled( false );
	actionDontInstallSourceRpm->setEnabled( false );
    }
}


void
YQPkgList::askExportList() const
{
    QString filename = YQApplication::askForSaveFileName( "pkglist.txt",	// startsWith
							  "*.txt",		// filter
							  _( "Export Package List" ) );
    if ( ! filename.isEmpty() )
	exportList( filename, true );
}


void
YQPkgList::exportList( const QString filename, bool interactive ) const
{
    // Open file

    QFile file(filename);
    file.open(QIODevice::WriteOnly);

    if ( file.error() != QFile::NoError )
    {
	yuiError() << "Can't open file " << filename << std::endl;

	if ( interactive )
	{
	    // Post error popup.

	    QMessageBox::warning( 0,						// parent
				  _( "Error" ),					// caption
				  _( "Cannot open file %1" ).arg( filename ),
				  QMessageBox::Ok | QMessageBox::Default,	// button0
				  QMessageBox::NoButton,			// button1
				  QMessageBox::NoButton );			// button2
	}
	return;
    }


    //
    // Write header
    //

    // Format the header line with QString::sprintf() because plain stdio
    // fprintf() is not UTF-8 aware - it will count multi-byte characters
    // wrong, so the formatting will be broken.

    QString header;
    header.sprintf( "# %-18s %-30s | %-40s | %-25s | %10s\n\n",
		    (const char *) _( "Status"      ).toUtf8(),
		    (const char *) _( "Package"     ).toUtf8(),
		    (const char *) _( "Summary"     ).toUtf8(),
		    (const char *) _( "Installed (Available)" ).toUtf8(),
		    (const char *) _( "Size"        ).toUtf8()
		    );
    file.write(header.toUtf8());


    // Write all items

    QTreeWidgetItemIterator it((QTreeWidget*) this);
    
    while (*it)
    {
        const QTreeWidgetItem* item(*it);
        const YQPkgListItem *  pkg = dynamic_cast<const YQPkgListItem *> (item);

        if ( pkg )
        {
            QString version = pkg->text( versionCol() );
            if ( version.isEmpty() ) version = "---";

            QString summary = pkg->text( summaryCol() );
            if ( summary.isEmpty() ) summary = "---";
            if ( summary.size() > 40 )
            {
                summary.truncate(40-3);
                summary += "...";
            }

            QString status = "[" + statusText( pkg->status() ) + "]";
            QString format;
	    format.sprintf("%-20s %-30s | %-40s | %-25s | %10s\n",
		     (const char*) status.toUtf8(),
		     (const char*) pkg->text( nameCol() ).toUtf8(),
		     (const char*) summary.toUtf8(),
		     (const char*) version.toUtf8(),
		     (const char*) pkg->text( sizeCol() ).toUtf8() 
		     );
            file.write(format.toUtf8());
	}
        ++it;
    }

    // Clean up

    if ( file.isOpen() )
        file.close();
}


int
YQPkgList::globalSetPkgStatus( ZyppStatus newStatus, bool force, bool countOnly )
{
    YQUI::ui()->busyCursor();
    int changedCount = 0;

    for ( ZyppPoolIterator it = zyppPkgBegin();
	  it != zyppPkgEnd();
	  ++it )
    {
	ZyppSel	   selectable = *it;
	ZyppStatus oldStatus  = selectable->status();

	if ( newStatus != oldStatus )
	{
	    bool doChange = false;

	    switch ( newStatus )
	    {
		case S_KeepInstalled:
		case S_Del:
		case S_AutoDel:
		case S_Protected:
		    doChange = !selectable->installedEmpty();
		    break;

		case S_Update:
		case S_AutoUpdate:

		    if ( force )
		    {
			doChange = !selectable->installedEmpty();
		    }
		    else // don't force - update only if useful (if candidate is newer)
		    {
			const ZyppObj candidate = selectable->candidateObj();
			const ZyppObj installed = selectable->installedObj();

			if ( candidate && installed )
			{
			    doChange = ( installed->edition() < candidate->edition() );
			}
		    }
		    break;

		case S_Install:
		case S_AutoInstall:
		case S_NoInst:
		case S_Taboo:
		    doChange = selectable->installedEmpty();
		    break;
	    }

	    if ( doChange )
	    {
		if ( ! countOnly && oldStatus != S_Protected )
		    selectable->setStatus( newStatus );

		changedCount++;
		// yuiMilestone() << "Updating " << selectable->name() << std::endl;
	    }
	}
    }

    if ( changedCount > 0 && ! countOnly )
    {
	emit updateItemStates();
	emit updatePackages();
	emit statusChanged();
    }

    YQUI::ui()->normalCursor();

    return changedCount;
}






YQPkgListItem::YQPkgListItem( YQPkgList * 		pkgList,
			      ZyppSel	selectable,
			      ZyppPkg 	zyppPkg )
    : YQPkgObjListItem( pkgList, selectable, zyppPkg )
    , _pkgList( pkgList )
    , _zyppPkg( zyppPkg )
    , _dimmed( false )
{
    if ( ! _zyppPkg )
	_zyppPkg = tryCastToZyppPkg( selectable->theObj() );

    setSourceRpmIcon();

    setTextAlignment( sizeCol(), Qt::AlignRight );
}


YQPkgListItem::~YQPkgListItem()
{
    // NOP
}


void
YQPkgListItem::updateData()
{
    YQPkgObjListItem::updateData();
    setSourceRpmIcon();
}


bool
YQPkgListItem::hasSourceRpm() const
{
    if ( ! selectable() )
	return false;

#ifdef FIXME
    return selectable()->providesSources();
#else
    return false;
#endif
}


bool
YQPkgListItem::installSourceRpm() const
{
    if ( ! selectable() )
	return false;

#ifdef FIXME
    if ( ! selectable()->providesSources() )
	return false;

    return selectable()->source_install();
#else
    return false;
#endif
}


void
YQPkgListItem::setSourceRpmIcon()
{
    if ( srpmStatusCol() < 0 )
	return;

    QPixmap icon;

    if ( hasSourceRpm() )
    {

	if ( editable() && _pkgObjList->editable() )
	{
	    icon = installSourceRpm() ?
		YQIconPool::pkgInstall() :
		YQIconPool::pkgNoInst();
	}
	else
	{
	    icon = installSourceRpm() ?
		YQIconPool::disabledPkgInstall() :
		YQIconPool::disabledPkgNoInst();
	}
    }
    setData( srpmStatusCol(), Qt::DecorationRole, icon );
}


void
YQPkgListItem::setInstallSourceRpm( bool installSourceRpm )
{
    if ( hasSourceRpm() )
    {
#ifdef FIXME
	if ( selectable() )
	    selectable()->set_source_install( installSourceRpm );
#endif
    }

    setSourceRpmIcon();
}


void
YQPkgListItem::toggleSourceRpmStatus()
{
    setInstallSourceRpm( ! installSourceRpm() );
}


QString
YQPkgListItem::toolTip( int col )
{
    QString text;
    QString name = _zyppObj->name().c_str();

    if ( col == statusCol() )
    {
	text = YQPkgObjListItem::toolTip( col );
    }
    else if ( col == srpmStatusCol() )
    {
	text = name + "\n\n";

	if ( hasSourceRpm() )
	{
	    text += installSourceRpm() ?
		_( "Install Sources" ) :
		_( "Do Not Install Sources" );
	}
	else
	{
	    text += _( "No Sources Available" );
	}
    }
    else
    {
	text = name + "\n\n";

	QString installed;
	QString candidate;

	if ( !selectable()->installedEmpty() )
	{
	    installed  = selectable()->installedObj()->edition().asString().c_str();
	    installed += "-";
	    installed +=  selectable()->installedObj()->arch().asString().c_str();
	    installed  = _( "Installed Version: %1" ).arg( installed );
	}

	if (  selectable()->hasCandidateObj() )
	{
	    candidate  = selectable()->candidateObj()->edition().asString().c_str();
	    candidate += "-";
	    candidate +=  selectable()->candidateObj()->arch().asString().c_str();
	}

	if ( !selectable()->installedEmpty() )
	{
	    text += installed + "\n";

	    if ( selectable()->hasCandidateObj() )
	    {
		// Translators: This is the relation between two versions of one package
		// if both versions are the same, e.g., both "1.2.3-42", "1.2.3-42"
		QString relation = _( "same" );

		if ( _candidateIsNewer )	relation = _( "newer" );
		if ( _installedIsNewer )	relation = _( "older" );

		// Translators: %1 is the version, %2 is one of "newer", "older", "same"
		text += _( "Available Version: %1 (%2)" ).arg( candidate ).arg( relation );
	    }
	    else
	    {
		text += _( "Not available for installation" );
	    }
	}
	else // not installed
	{
	    text += candidate;
	}
    }

    return text;
}



bool YQPkgListItem::operator< ( const QTreeWidgetItem & otherListViewItem ) const
{
    const YQPkgListItem * other = dynamic_cast<const YQPkgListItem *> (&otherListViewItem);

    int col = treeWidget()->sortColumn();
    if ( col == srpmStatusCol() )
    {
	if ( other )
	{
	    int thisPoints  = ( this->hasSourceRpm()  ? 1 : 0 ) + ( this->installSourceRpm()  ? 1 : 0 );
	    int otherPoints = ( other->hasSourceRpm() ? 1 : 0 ) + ( other->installSourceRpm() ? 1 : 0 );

	    // Intentionally inverting order: Pkgs with source RPMs are more interesting than without.
	    return ( thisPoints < otherPoints );
	}
    }

    // Fallback: Use parent class method
    return YQPkgObjListItem::operator<( otherListViewItem );
}

#include "YQPkgList.moc"
