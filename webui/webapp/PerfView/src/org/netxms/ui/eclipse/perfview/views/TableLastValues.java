/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2011 Victor Kirhenshtein
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.ui.eclipse.perfview.views;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.Arrays;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.GroupMarker;
import org.eclipse.jface.action.IMenuListener;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.viewers.ViewerCell;
import org.eclipse.rap.rwt.RWT;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.part.ViewPart;
import org.netxms.client.NXCSession;
import org.netxms.client.Table;
import org.netxms.client.objects.AbstractObject;
import org.netxms.ui.eclipse.actions.ExportToCsvAction;
import org.netxms.ui.eclipse.actions.RefreshAction;
import org.netxms.ui.eclipse.charts.api.DataComparisonChart;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.perfview.Activator;
import org.netxms.ui.eclipse.perfview.views.helpers.CellSelectionManager;
import org.netxms.ui.eclipse.perfview.views.helpers.TableContentProvider;
import org.netxms.ui.eclipse.perfview.views.helpers.TableItemComparator;
import org.netxms.ui.eclipse.perfview.views.helpers.TableLabelProvider;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.SortableTableViewer;

/**
 * Display results of table tool execution
 */
public class TableLastValues extends ViewPart
{
	public static final String ID = "org.netxms.ui.eclipse.perfview.views.TableLastValues";
	
	private NXCSession session;
	private long nodeId;
	private long dciId;
	private long uniqueId = 1;
	private Table currentData = null;
	private SortableTableViewer viewer;
	private CellSelectionManager cellSelectionManager;
	private String nodeName;
	private Action actionRefresh;
	private Action actionExportAllToCsv;
	private Action actionShowLineChart;
	private Action actionShowBarChart;
	private Action actionShowPieChart;
	
	/* (non-Javadoc)
	 * @see org.eclipse.ui.part.ViewPart#init(org.eclipse.ui.IViewSite)
	 */
	@Override
	public void init(IViewSite site) throws PartInitException
	{
		super.init(site);
		
		session = (NXCSession)ConsoleSharedData.getSession();
		
		// Secondary ID must by in form nodeId&dciId
		String[] parts = site.getSecondaryId().split("&");
		if (parts.length != 2)
			throw new PartInitException("Internal error");
		
		nodeId = Long.parseLong(parts[0]);
		AbstractObject object = session.findObjectById(nodeId);
		if ((object == null) || (object.getObjectClass() != AbstractObject.OBJECT_NODE))
			throw new PartInitException("Invalid object ID");
		nodeName = object.getObjectName();
		
		dciId = Long.parseLong(parts[1]);
		
		setPartName(nodeName + ": [" + Long.toString(dciId) + "]");
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.part.WorkbenchPart#createPartControl(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	public void createPartControl(Composite parent)
	{
		viewer = new SortableTableViewer(parent, SWT.FULL_SELECTION | SWT.MULTI);
		viewer.getTable().setData(RWT.MARKUP_ENABLED, Boolean.TRUE);
		viewer.setContentProvider(new TableContentProvider());
		viewer.setLabelProvider(new TableLabelProvider());
		cellSelectionManager = new CellSelectionManager(viewer);

		createActions();
		contributeToActionBars();
		createPopupMenu();
		
		refreshTable();
	}

	/**
	 * Create actions
	 */
	private void createActions()
	{
		actionRefresh = new RefreshAction(this) {
			@Override
			public void run()
			{
				refreshTable();
			}
		};
		
		actionShowLineChart = new Action("&Line chart", Activator.getImageDescriptor("icons/chart_line.png")) {
			@Override
			public void run()
			{
				showLineChart();
			}
		};

		actionShowBarChart = new Action("&Bar chart", Activator.getImageDescriptor("icons/chart_bar.png")) {
			@Override
			public void run()
			{
				showDataComparisonChart(DataComparisonChart.BAR_CHART);
			}
		};

		actionShowPieChart = new Action("&Pie chart", Activator.getImageDescriptor("icons/chart_pie.png")) {
			@Override
			public void run()
			{
				showDataComparisonChart(DataComparisonChart.PIE_CHART);
			}
		};

		actionExportAllToCsv = new ExportToCsvAction(this, viewer, false);
	}

	/**
	 * Contribute actions to action bar
	 */
	private void contributeToActionBars()
	{
		IActionBars bars = getViewSite().getActionBars();
		fillLocalPullDown(bars.getMenuManager());
		fillLocalToolBar(bars.getToolBarManager());
	}

	/**
	 * Fill local pull-down menu
	 * 
	 * @param manager
	 *           Menu manager for pull-down menu
	 */
	private void fillLocalPullDown(IMenuManager manager)
	{
		manager.add(actionExportAllToCsv);
		manager.add(new Separator());
		manager.add(actionRefresh);
	}

	/**
	 * Fill local tool bar
	 * 
	 * @param manager
	 *           Menu manager for local toolbar
	 */
	private void fillLocalToolBar(IToolBarManager manager)
	{
		manager.add(actionExportAllToCsv);
		manager.add(actionRefresh);
	}

	/**
	 * Create pop-up menu
	 */
	private void createPopupMenu()
	{
		// Create menu manager.
		MenuManager menuMgr = new MenuManager();
		menuMgr.setRemoveAllWhenShown(true);
		menuMgr.addMenuListener(new IMenuListener() {
			public void menuAboutToShow(IMenuManager mgr)
			{
				fillContextMenu(mgr);
			}
		});

		// Create menu.
		Menu menu = menuMgr.createContextMenu(viewer.getControl());
		viewer.getControl().setMenu(menu);

		// Register menu for extension.
		getSite().registerContextMenu(menuMgr, viewer);
	}


	/**
	 * Fill context menu
	 * @param manager
	 */
	private void fillContextMenu(IMenuManager manager)
	{
		manager.add(actionShowLineChart);
		manager.add(actionShowBarChart);
		manager.add(actionShowPieChart);
		manager.add(new Separator());
		manager.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
	}
	
	/* (non-Javadoc)
	 * @see org.eclipse.ui.part.WorkbenchPart#setFocus()
	 */
	@Override
	public void setFocus()
	{
		viewer.getTable().setFocus();
	}

	/**
	 * Refresh table
	 */
	public void refreshTable()
	{
		viewer.setInput(null);
		new ConsoleJob("Loading data for table DCI " + Long.toString(dciId), this, Activator.PLUGIN_ID, null) {
			@Override
			protected void runInternal(IProgressMonitor monitor) throws Exception
			{
				final Table table = session.getTableLastValues(nodeId, dciId);
				runInUIThread(new Runnable() {
					@Override
					public void run()
					{
						setPartName(nodeName + ": " + table.getTitle());
						updateViewer(table);
					}
				});
			}

			@Override
			protected String getErrorMessage()
			{
				return "Cannot get data for table DCI " + Long.toString(dciId);
			}
		}.start();
	}
	
	/**
	 * Update viewer with fresh table data
	 * 
	 * @param table table
	 */
	private void updateViewer(final Table table)
	{
		if (!viewer.isInitialized())
		{
			final String[] names = table.getColumnNames();
			final int[] widths = new int[names.length];
			Arrays.fill(widths, 150);
			viewer.createColumns(names, widths, 0, SWT.UP);
			WidgetHelper.restoreTableViewerSettings(viewer, Activator.getDefault().getDialogSettings(), "TableLastValues");
			viewer.getTable().addDisposeListener(new DisposeListener() {
				@Override
				public void widgetDisposed(DisposeEvent e)
				{
					WidgetHelper.saveTableViewerSettings(viewer, Activator.getDefault().getDialogSettings(), "TableLastValues");
				}
			});
			viewer.setComparator(new TableItemComparator(table.getColumnFormats()));
		}
		viewer.setInput(table);
		currentData = table;
	}
	
	/**
	 * Show line chart
	 */
	private void showLineChart()
	{
		if (currentData == null)
			return;
		
		ViewerCell[] cells = cellSelectionManager.getSelectedCells();
		if (cells.length == 0)
			return;
		
		int instanceColumnIndex = currentData.getColumnIndex(currentData.getInstanceColumn());
		String id = Long.toString(uniqueId++);
		for(int i = 0; i < cells.length; i++)
		{
			String columnName = currentData.getColumnName(cells[i].getColumnIndex());
			String instance = cells[i].getViewerRow().getText(instanceColumnIndex);
			int dataType = currentData.getColumnFormat(cells[i].getColumnIndex());
			int source = currentData.getSource();
			
			id += "&" + Long.toString(nodeId) + "@" + Long.toString(dciId) + "@" + 
					Integer.toString(source) + "@" + Integer.toString(dataType) + "@" + 
					safeEncode(currentData.getTitle()) + "@" + safeEncode(columnName + ": " + instance) + 
					"@" + safeEncode(instance) + "@" + safeEncode(columnName);
		}
		
		try
		{
			getSite().getPage().showView(HistoricalGraphView.ID, id, IWorkbenchPage.VIEW_ACTIVATE);
		}
		catch(Exception e)
		{
			MessageDialogHelper.openError(getSite().getShell(), "Error", String.format("Error opening view: %s", e.getLocalizedMessage()));
		}
	}
	
	/**
	 * Show line chart
	 */
	private void showDataComparisonChart(int chartType)
	{
		if (currentData == null)
			return;
		
		ViewerCell[] cells = cellSelectionManager.getSelectedCells();
		if (cells.length == 0)
			return;
		
		int instanceColumnIndex = currentData.getColumnIndex(currentData.getInstanceColumn());
		String id = Long.toString(uniqueId++) + "&" + Integer.toString(chartType);
		for(int i = 0; i < cells.length; i++)
		{
			String columnName = currentData.getColumnName(cells[i].getColumnIndex());
			String instance = cells[i].getViewerRow().getText(instanceColumnIndex);
			int dataType = currentData.getColumnFormat(cells[i].getColumnIndex());
			int source = currentData.getSource();
			
			id += "&" + Long.toString(nodeId) + "@" + Long.toString(dciId) + "@" + 
					Integer.toString(source) + "@" + Integer.toString(dataType) + "@" + 
					safeEncode(currentData.getTitle()) + "@" + safeEncode(columnName + ": " + instance) + 
					"@" + safeEncode(instance) + "@" + safeEncode(columnName);
		}
		
		try
		{
			getSite().getPage().showView(DataComparisonView.ID, id, IWorkbenchPage.VIEW_ACTIVATE);
		}
		catch(Exception e)
		{
			MessageDialogHelper.openError(getSite().getShell(), "Error", String.format("Error opening view: %s", e.getLocalizedMessage()));
		}
	}
	
	/**
	 * @param text
	 * @return
	 */
	private static String safeEncode(String text)
	{
		if (text == null)
			return "";
		
		try
		{
			return URLEncoder.encode(text, "UTF-8");
		}
		catch(UnsupportedEncodingException e)
		{
			return "none";
		}
	}
}
