/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2020 Raden Solutions
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
package org.netxms.nxmc;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Properties;
import org.eclipse.swt.graphics.Point;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Local preference store
 */
public class PreferenceStore
{
   private static PreferenceStore instance = null;

   /**
    * Open local store
    */
   protected static void open(String stateDir)
   {
      instance = new PreferenceStore(new File(stateDir + File.separator + "nxmc.preferences"));
   }

   /**
    * Get instance of preference store
    * 
    * @return instance of preference store
    */
   public static PreferenceStore getInstance()
   {
      return instance;
   }

   private File storeFile;
   private Properties properties;
   private Logger logger = LoggerFactory.getLogger(PreferenceStore.class);

   /**
    * Default constructor
    */
   private PreferenceStore(File storeFile)
   {
      this.storeFile = storeFile;
      properties = new Properties();
      if (storeFile.exists())
      {
         FileReader reader = null;
         try
         {
            reader = new FileReader(storeFile);
            properties.load(reader);
         }
         catch(Exception e)
         {
            logger.error("Error reading local preferences from " + storeFile.getAbsolutePath(), e);
         }
         finally
         {
            if (reader != null)
            {
               try
               {
                  reader.close();
               }
               catch(IOException e)
               {
               }
            }
         }
      }
   }

   /**
    * Save preference store
    */
   private void save()
   {
      FileWriter writer = null;
      try
      {
         writer = new FileWriter(storeFile);
         properties.store(writer, "NXMC local preferences");
      }
      catch(Exception e)
      {
         logger.error("Error writing local preferences to " + storeFile.getAbsolutePath(), e);
      }
      finally
      {
         if (writer != null)
         {
            try
            {
               writer.close();
            }
            catch(IOException e)
            {
            }
         }
      }
   }

   /**
    * Get property as string.
    * 
    * @param name property name
    * @return property value or null if not found
    */
   public String getAsString(String name)
   {
      return properties.getProperty(name);
   }

   /**
    * Get property as string.
    * 
    * @param name property name
    * @param defaultValue default value
    * @return property value or default value if not found
    */
   public String getAsString(String name, String defaultValue)
   {
      return properties.getProperty(name, defaultValue);
   }

   /**
    * Get property as boolean.
    * 
    * @param name property name
    * @param defaultValue default value
    * @return property value or default value if not found
    */
   public boolean getAsBoolean(String name, boolean defaultValue)
   {
      String v = properties.getProperty(name);
      return (v != null) ? Boolean.parseBoolean(v) : defaultValue;
   }

   /**
    * Get property as integer.
    * 
    * @param name property name
    * @param defaultValue default value
    * @return property value or default value if not found or cannot be interpreted as integer
    */
   public int getAsInteger(String name, int defaultValue)
   {
      String v = properties.getProperty(name);
      if (v == null)
         return defaultValue;
      try
      {
         return Integer.parseInt(v);
      }
      catch(NumberFormatException e)
      {
         return defaultValue;
      }
   }

   /**
    * Get property as long integer.
    * 
    * @param name property name
    * @param defaultValue default value
    * @return property value or default value if not found or cannot be interpreted as long integer
    */
   public long getAsLong(String name, long defaultValue)
   {
      String v = properties.getProperty(name);
      if (v == null)
         return defaultValue;
      try
      {
         return Long.parseLong(v);
      }
      catch(NumberFormatException e)
      {
         return defaultValue;
      }
   }

   /**
    * Get property as point object.
    * 
    * @param name property name
    * @param defaultValue default value
    * @return property value or default value if not found or cannot be interpreted as a point
    */
   public Point getAsPoint(String name, Point defaultValue)
   {
      return getAsPoint(name, defaultValue.x, defaultValue.y);
   }

   /**
    * Get property as point object.
    * 
    * @param name property name
    * @param defaultX default value for point's X value
    * @param defaultY default value for point's Y value
    * @return property value or default value if not found or cannot be interpreted as a point
    */
   public Point getAsPoint(String name, int defaultX, int defaultY)
   {
      String v = properties.getProperty(name);
      if (v == null)
         return new Point(defaultX, defaultY);

      String[] parts = v.split(",");
      if (parts.length != 2)
         return new Point(defaultX, defaultY);

      try
      {
         return new Point(Integer.parseInt(parts[0]), Integer.parseInt(parts[1]));
      }
      catch(NumberFormatException e)
      {
         return new Point(defaultX, defaultY);
      }
   }

   /**
    * Get property as list of strings
    * 
    * @param name property name
    * @return list of strings (empty list if property was not set)
    */
   public List<String> getAsStringList(String name)
   {
      int count = getAsInteger(name + ".Count", 0);
      List<String> list = new ArrayList<String>(count);
      for(int i = 0; i < count; i++)
         list.add(getAsString(name + "." + Integer.toString(i), ""));
      return list;
   }

   /**
    * Get property as array of strings
    * 
    * @param name property name
    * @return list of strings (empty array if property was not set)
    */
   public String[] getAsStringArray(String name)
   {
      int count = getAsInteger(name + ".Count", 0);
      String[] list = new String[count];
      for(int i = 0; i < count; i++)
         list[i] = getAsString(name + "." + Integer.toString(i), "");
      return list;
   }

   /**
    * Set property.
    * 
    * @param name property name
    * @param value new property value
    */
   public void set(String name, String value)
   {
      properties.setProperty(name, value);
      save();
   }

   /**
    * Set property.
    * 
    * @param name property name
    * @param value new property value
    */
   public void set(String name, boolean value)
   {
      properties.setProperty(name, Boolean.toString(value));
      save();
   }

   /**
    * Set property.
    * 
    * @param name property name
    * @param value new property value
    */
   public void set(String name, int value)
   {
      properties.setProperty(name, Integer.toString(value));
      save();
   }

   /**
    * Set property.
    * 
    * @param name property name
    * @param value new property value
    */
   public void set(String name, long value)
   {
      properties.setProperty(name, Long.toString(value));
      save();
   }

   /**
    * Set property.
    * 
    * @param name property name
    * @param value new property value
    */
   public void set(String name, Point value)
   {
      properties.setProperty(name, Integer.toString(value.x) + "," + Integer.toString(value.y));
      save();
   }

   /**
    * Set property.
    * 
    * @param name property name
    * @param value new property value
    */
   public void set(String name, Collection<String> value)
   {
      properties.setProperty(name + ".Count", Integer.toString(value.size()));
      int index = 0;
      for(String s : value)
         properties.setProperty(name + "." + Integer.toString(index), s);
      save();
   }
}
