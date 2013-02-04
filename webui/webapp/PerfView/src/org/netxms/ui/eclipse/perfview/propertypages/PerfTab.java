/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2010 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.perfview.propertypages;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.preference.ColorSelector;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.layout.RowLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.ui.dialogs.PropertyPage;
import org.eclipse.ui.progress.UIJob;
import org.netxms.client.datacollection.DataCollectionItem;
import org.netxms.ui.eclipse.datacollection.widgets.DciSelector;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.perfview.Activator;
import org.netxms.ui.eclipse.perfview.PerfTabGraphSettings;
import org.netxms.ui.eclipse.tools.ColorConverter;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.LabeledText;

/**
 * DCI property page for Performance Tab settings
 */
public class PerfTab extends PropertyPage
{
	private static final long serialVersionUID = 1L;

	private DataCollectionItem dci;
	private PerfTabGraphSettings settings;
	private Button checkShow;
	private LabeledText title;
	private LabeledText name;
	private ColorSelector color;
	private Combo type;
	private Spinner orderNumber;
	private Button checkShowThresholds;
	private DciSelector parentDci;
	
	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	protected Control createContents(Composite parent)
	{
		dci = (DataCollectionItem)getElement().getAdapter(DataCollectionItem.class);
		try
		{
			settings = PerfTabGraphSettings.createFromXml(dci.getPerfTabSettings());
		}
		catch(Exception e)
		{
			settings = new PerfTabGraphSettings();	// Create default empty settings
		}
		
		Composite dialogArea = new Composite(parent, SWT.NONE);
		
		GridLayout layout = new GridLayout();
		layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
		layout.marginWidth = 0;
		layout.marginHeight = 0;
		layout.numColumns = 4;
      dialogArea.setLayout(layout);
      
      checkShow = new Button(dialogArea, SWT.CHECK);
      checkShow.setText("&Show on performance tab");
      checkShow.setSelection(settings.isEnabled());
      GridData gd = new GridData();
      gd.horizontalSpan = layout.numColumns;
      checkShow.setLayoutData(gd);
      
      title = new LabeledText(dialogArea, SWT.NONE);
      title.setLabel("Title");
      title.setText(settings.getTitle());
      gd = new GridData();
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalAlignment = SWT.FILL;
      title.setLayoutData(gd);
      
      Composite colors = new Composite(dialogArea, SWT.NONE);
      colors.setLayout(new RowLayout(SWT.VERTICAL));
      new Label(colors, SWT.NONE).setText("Color");
      color = new ColorSelector(colors);
      color.setColorValue(ColorConverter.rgbFromInt(settings.getColorAsInt()));
      
      type = WidgetHelper.createLabeledCombo(dialogArea, SWT.READ_ONLY, "Type", new GridData(SWT.LEFT, SWT.CENTER, false, false));
      type.add("Line");
      type.add("Area");
      type.select(settings.getType());
      
      orderNumber = WidgetHelper.createLabeledSpinner(dialogArea, SWT.BORDER, "Order", 0, 65535, new GridData(SWT.LEFT, SWT.CENTER, false, false));
      orderNumber.setSelection(settings.getOrder());

      parentDci = new DciSelector(dialogArea, SWT.NONE, false);
      parentDci.setDciId(dci.getNodeId(), settings.getParentDciId());
      parentDci.setFixedNode(true);
      parentDci.setLabel("Attach to another DCI");
      gd = new GridData();
      gd.horizontalSpan = layout.numColumns;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalAlignment = SWT.FILL;
      parentDci.setLayoutData(gd);

      name = new LabeledText(dialogArea, SWT.NONE);
      name.setLabel("Name in legend");
      name.setText(settings.getName());
      gd = new GridData();
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalAlignment = SWT.FILL;
      gd.horizontalSpan = layout.numColumns;
      name.setLayoutData(gd);
      
      checkShowThresholds = new Button(dialogArea, SWT.CHECK);
      checkShowThresholds.setText("&Show thresholds on graph");
      checkShowThresholds.setSelection(settings.isShowThresholds());
      gd = new GridData();
      gd.horizontalSpan = layout.numColumns;
      checkShowThresholds.setLayoutData(gd);
      
      return dialogArea;
	}
	
	/**
	 * Apply changes
	 * 
	 * @param isApply true if APPLY button was used
	 */
	private void applyChanges(final boolean isApply)
	{
		if (isApply)
			setValid(false);
		
		settings.setEnabled(checkShow.getSelection());
		settings.setTitle(title.getText());
		settings.setName(name.getText());
		settings.setColor(ColorConverter.rgbToInt(color.getColorValue()));
		settings.setType(type.getSelectionIndex());
		settings.setOrder(orderNumber.getSelection());
		settings.setShowThresholds(checkShowThresholds.getSelection());

		settings.setParentDciId(parentDci.getDciId());
		
		try
		{
			dci.setPerfTabSettings(settings.createXml());
		}
		catch(Exception e)
		{
			dci.setPerfTabSettings(null);
		}
		
		ConsoleJob job = new ConsoleJob("Update performance tab settings for DCI " + dci.getId(), null, Activator.PLUGIN_ID, null) {
			@Override
			protected String getErrorMessage()
			{
				return "Cannot update performance tab settings for DCI " + dci.getId();
			}

			@Override
			protected void runInternal(IProgressMonitor monitor) throws Exception
			{
				dci.getOwner().modifyObject(dci);
			}

			/* (non-Javadoc)
			 * @see org.netxms.ui.eclipse.jobs.ConsoleJob#jobFinalize()
			 */
			@Override
			protected void jobFinalize()
			{
				if (isApply)
				{
					new UIJob("Update \"Performance Tab\" property page") {
						@Override
						public IStatus runInUIThread(IProgressMonitor monitor)
						{
							PerfTab.this.setValid(true);
							return Status.OK_STATUS;
						}
					}.schedule();
				}
			}
		};
		job.start();
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performApply()
	 */
	@Override
	protected void performApply()
	{
		applyChanges(true);
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performOk()
	 */
	@Override
	public boolean performOk()
	{
		applyChanges(false);
		return true;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performDefaults()
	 */
	@Override
	protected void performDefaults()
	{
		super.performDefaults();
		PerfTabGraphSettings defaults = new PerfTabGraphSettings();
		checkShow.setSelection(defaults.isEnabled());
		title.setText(defaults.getTitle());
		color.setColorValue(ColorConverter.rgbFromInt(defaults.getColorAsInt()));
		parentDci.setDciId(0, 0);
	}
}
