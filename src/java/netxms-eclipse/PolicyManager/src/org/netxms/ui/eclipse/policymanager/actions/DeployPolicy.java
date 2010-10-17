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
package org.netxms.ui.eclipse.policymanager.actions;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.IAction;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.TreeSelection;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IObjectActionDelegate;
import org.eclipse.ui.IWorkbenchPart;
import org.netxms.client.NXCSession;
import org.netxms.client.objects.AgentPolicy;
import org.netxms.client.objects.GenericObject;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.objectbrowser.dialogs.ObjectSelectionDialog;
import org.netxms.ui.eclipse.policymanager.Activator;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;

/**
 * Action: deploy policy on agent
 *
 */
public class DeployPolicy implements IObjectActionDelegate
{
	private Shell shell;
	private GenericObject currentObject;
	
	/* (non-Javadoc)
	 * @see org.eclipse.ui.IObjectActionDelegate#setActivePart(org.eclipse.jface.action.IAction, org.eclipse.ui.IWorkbenchPart)
	 */
	@Override
	public void setActivePart(IAction action, IWorkbenchPart targetPart)
	{
		shell = targetPart.getSite().getShell();
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.IActionDelegate#run(org.eclipse.jface.action.IAction)
	 */
	@Override
	public void run(IAction action)
	{
		// Read custom root objects
		long[] rootObjects = null;
		Object value = ConsoleSharedData.getInstance().getProperty("PolicyManager.rootObjects");
		if ((value != null) && (value instanceof long[]))
		{
			rootObjects = (long[])value;
		}
		
		final ObjectSelectionDialog dlg = new ObjectSelectionDialog(shell, rootObjects, ObjectSelectionDialog.createNodeSelectionFilter());
		if (dlg.open() == Window.OK)
		{
			new ConsoleJob("Deploy agent policy", null, Activator.PLUGIN_ID, null) {
				@Override
				protected void runInternal(IProgressMonitor monitor) throws Exception
				{
					NXCSession session = ConsoleSharedData.getInstance().getSession();
					GenericObject[] nodeList = dlg.getSelectedObjects(GenericObject.OBJECT_NODE);
					for(int i = 0; i < nodeList.length; i++)
						session.deployAgentPolicy(currentObject.getObjectId(), nodeList[i].getObjectId());
				}

				@Override
				protected String getErrorMessage()
				{
					return "Cannot deploy agent policy";
				}
			}.start();
		}
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.IActionDelegate#selectionChanged(org.eclipse.jface.action.IAction, org.eclipse.jface.viewers.ISelection)
	 */
	@Override
	public void selectionChanged(IAction action, ISelection selection)
	{
		if (selection instanceof TreeSelection)
		{
			currentObject = (GenericObject)((TreeSelection)selection).getFirstElement();
			action.setEnabled(currentObject instanceof AgentPolicy);
		}
	}
}
