//
// Licensed under the Academic Free License version 3.0
//

package sedonac;

import java.io.*;
import java.util.ArrayList;

/**
 * DiagnosticsJson writes the outcome of a compile as JSON for tools
 * (sedonac -json file):
 *
 * <pre>
 * {
 *   "version": 1,
 *   "ok": false,
 *   "diagnostics": [
 *     { "severity": "error", "message": "...", "file": "app.sax", "line": 27, "col": 5 }
 *   ]
 * }
 * </pre>
 *
 * file, line and col are left out when unknown (line or col 0).
 */
public class DiagnosticsJson
{

  public DiagnosticsJson(boolean ok)
  {
    this.ok = ok;
  }

  /**
   * Add a CompilerException; the message of its cause, if any, is
   * appended, since it usually says what went wrong.
   */
  public void add(String severity, CompilerException e)
  {
    String msg = e.getMessage();
    if (e.cause != null && e.cause.getMessage() != null)
      msg += ": " + e.cause.getMessage();
    add(severity, msg, e.location);
  }

  public void add(String severity, String message, Location loc)
  {
    items.add(new Object[] { severity, message, loc });
  }

  public void write(File file)
    throws IOException
  {
    Writer out = new OutputStreamWriter(new FileOutputStream(file), "UTF-8");
    try
    {
      out.write(toString());
    }
    finally
    {
      out.close();
    }
  }

  public String toString()
  {
    StringBuffer s = new StringBuffer();
    s.append("{\n  \"version\": 1,\n  \"ok\": ").append(ok).append(",\n  \"diagnostics\": [");
    for (int i=0; i<items.size(); ++i)
    {
      Object[] d = (Object[])items.get(i);
      Location loc = (Location)d[2];
      s.append(i == 0 ? "\n" : ",\n");
      s.append("    { \"severity\": ").append(str((String)d[0]));
      s.append(", \"message\": ").append(str((String)d[1]));
      if (loc != null && loc.file != null) s.append(", \"file\": ").append(str(loc.file));
      if (loc != null && loc.line > 0) s.append(", \"line\": ").append(loc.line);
      if (loc != null && loc.col > 0) s.append(", \"col\": ").append(loc.col);
      s.append(" }");
    }
    s.append(items.size() == 0 ? "]\n}\n" : "\n  ]\n}\n");
    return s.toString();
  }

  /**
   * A JSON string literal.
   */
  static String str(String v)
  {
    if (v == null) return "null";
    StringBuffer s = new StringBuffer(v.length() + 2);
    s.append('"');
    for (int i=0; i<v.length(); ++i)
    {
      char c = v.charAt(i);
      switch (c)
      {
        case '"':  s.append("\\\""); break;
        case '\\': s.append("\\\\"); break;
        case '\n': s.append("\\n"); break;
        case '\r': s.append("\\r"); break;
        case '\t': s.append("\\t"); break;
        default:
          if (c < 0x20)
          {
            String hex = Integer.toHexString(c);
            s.append("\\u");
            for (int j=hex.length(); j<4; ++j) s.append('0');
            s.append(hex);
          }
          else s.append(c);
      }
    }
    return s.append('"').toString();
  }

  private final boolean ok;
  private final ArrayList items = new ArrayList();

}
