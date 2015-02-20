//========================================================================
//
// pdfinfo.cc
//
// Copyright 1998-2003 Glyph & Cog, LLC
//
// Includes modifications by the Zotero project (zotero.org)
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "parseargs.h"
#include "GString.h"
#include "gmem.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "CharTypes.h"
#include "UnicodeMap.h"
#include "PDFDocEncoding.h"
#include "Error.h"
#include "config.h"

static void printInfoString(FILE *f, Dict *infoDict, char *key,
			    char *text, UnicodeMap *uMap);
static void printInfoDate(FILE *f, Dict *infoDict, char *key, char *text);
static void printBox(FILE *f, char *text, PDFRectangle *box);

static int firstPage = 1;
static int lastPage = 0;
static GBool printBoxes = gFalse;
static GBool printMetadata = gFalse;
static char textEncName[128] = "";
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static char cfgFileName[256] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,        0,
   "first page to convert"},
  {"-l",      argInt,      &lastPage,         0,
   "last page to convert"},
  {"-box",    argFlag,     &printBoxes,       0,
   "print the page bounding boxes"},
  {"-meta",   argFlag,     &printMetadata,    0,
   "print the document metadata (XML)"},
  {"-enc",    argString,   textEncName,    sizeof(textEncName),
   "output text encoding name"},
  {"-opw",    argString,   ownerPassword,  sizeof(ownerPassword),
   "owner password (for encrypted files)"},
  {"-upw",    argString,   userPassword,   sizeof(userPassword),
   "user password (for encrypted files)"},
  {"-cfg",        argString,      cfgFileName,    sizeof(cfgFileName),
   "configuration file to use in place of .xpdfrc"},
  {"-v",      argFlag,     &printVersion,  0,
   "print copyright and version info"},
  {"-h",      argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {"--help",  argFlag,     &printHelp,     0,
   "print usage information"},
  {"-?",      argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GString *fileName;
  GString *textFileName;
  GString *ownerPW, *userPW;
  UnicodeMap *uMap;
  Page *page;
  Object info;
  char buf[256];
  double w, h, wISO, hISO;
  FILE *f;
  FILE *sizef;
  GString *metadata;
  GBool ok;
  char *p;
  int exitCode;
  int pg, i;
  GBool multiPage;

  exitCode = 99;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) {
    fprintf(stderr, "pdfinfo version %s-zotero\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    fputs("Includes modifications by the Zotero project (http://zotero.org)\n", stderr);
    if (!printVersion) {
      printUsage("pdfinfo", "<PDF-file> [<text-file>]", argDesc);
    }
    goto err0;
  }
  fileName = new GString(argv[1]);

  // read config file
  globalParams = new GlobalParams(cfgFileName);
  if (textEncName[0]) {
    globalParams->setTextEncoding(textEncName);
  }

  // get mapping to output encoding
  if (!(uMap = globalParams->getTextEncoding())) {
    error(-1, "Couldn't get text encoding");
    delete fileName;
    goto err1;
  }

  // open PDF file
  if (ownerPassword[0] != '\001') {
    ownerPW = new GString(ownerPassword);
  } else {
    ownerPW = NULL;
  }
  if (userPassword[0] != '\001') {
    userPW = new GString(userPassword);
  } else {
    userPW = NULL;
  }
  doc = new PDFDoc(fileName, ownerPW, userPW);
  if (userPW) {
    delete userPW;
  }
  if (ownerPW) {
    delete ownerPW;
  }
  if (!doc->isOk()) {
    exitCode = 1;
    goto err2;
  }

  // construct text file name
  if (argc == 3) {
    textFileName = new GString(argv[2]);
  }
  else {
    textFileName = new GString("-");
  }
  
  // Open text file for writing
  if (!textFileName->cmp("-")) {
    f = stdout;
  } else {
    if (!(f = fopen(textFileName->getCString(), "ab"))) {
      error(-1, "Couldn't open text file '%s'", textFileName->getCString());
      exitCode = 2;
      goto err3;
    }
  }
  
  // get page range
  if (firstPage < 1) {
    firstPage = 1;
  }
  if (lastPage == 0) {
    multiPage = gFalse;
    lastPage = 1;
  } else {
    multiPage = gTrue;
  }
  if (lastPage < 1 || lastPage > doc->getNumPages()) {
    lastPage = doc->getNumPages();
  }

  // print doc info
  doc->getDocInfo(&info);
  if (info.isDict()) {
    printInfoString(f, info.getDict(), "Title",        "Title:          ", uMap);
    printInfoString(f, info.getDict(), "Subject",      "Subject:        ", uMap);
    printInfoString(f, info.getDict(), "Keywords",     "Keywords:       ", uMap);
    printInfoString(f, info.getDict(), "Author",       "Author:         ", uMap);
    printInfoString(f, info.getDict(), "Creator",      "Creator:        ", uMap);
    printInfoString(f, info.getDict(), "Producer",     "Producer:       ", uMap);
    printInfoDate(f, info.getDict(),   "CreationDate", "CreationDate:   ");
    printInfoDate(f, info.getDict(),   "ModDate",      "ModDate:        ");
  }
  info.free();

  // print tagging info
  fprintf(f, "Tagged:         %s\n",
	 doc->getStructTreeRoot()->isDict() ? "yes" : "no");

  // print page count
  fprintf(f, "Pages:          %d\n", doc->getNumPages());

  // print encryption info
  fprintf(f, "Encrypted:      ");
  if (doc->isEncrypted()) {
    fprintf(f, "yes (print:%s copy:%s change:%s addNotes:%s)\n",
	   doc->okToPrint(gTrue) ? "yes" : "no",
	   doc->okToCopy(gTrue) ? "yes" : "no",
	   doc->okToChange(gTrue) ? "yes" : "no",
	   doc->okToAddNotes(gTrue) ? "yes" : "no");
  } else {
    fprintf(f, "no\n");
  }

  // print page size
  for (pg = firstPage; pg <= lastPage; ++pg) {
    w = doc->getPageCropWidth(pg);
    h = doc->getPageCropHeight(pg);
    if (multiPage) {
      fprintf(f, "Page %4d size: %g x %g pts", pg, w, h);
    } else {
      fprintf(f, "Page size:      %g x %g pts", w, h);
    }
    if ((fabs(w - 612) < 0.1 && fabs(h - 792) < 0.1) ||
	(fabs(w - 792) < 0.1 && fabs(h - 612) < 0.1)) {
      fprintf(f, " (letter)");
    } else {
      hISO = sqrt(sqrt(2.0)) * 7200 / 2.54;
      wISO = hISO / sqrt(2.0);
      for (i = 0; i <= 6; ++i) {
	if ((fabs(w - wISO) < 1 && fabs(h - hISO) < 1) ||
	    (fabs(w - hISO) < 1 && fabs(h - wISO) < 1)) {
	  fprintf(f, " (A%d)", i);
	  break;
	}
	hISO = wISO;
	wISO /= sqrt(2.0);
      }
    }
    fprintf(f, "\n");
  } 

  // print the boxes
  if (printBoxes) {
    if (multiPage) {
      for (pg = firstPage; pg <= lastPage; ++pg) {
	page = doc->getCatalog()->getPage(pg);
	sprintf(buf, "Page %4d MediaBox: ", pg);
	printBox(f, buf, page->getMediaBox());
	sprintf(buf, "Page %4d CropBox:  ", pg);
	printBox(f, buf, page->getCropBox());
	sprintf(buf, "Page %4d BleedBox: ", pg);
	printBox(f, buf, page->getBleedBox());
	sprintf(buf, "Page %4d TrimBox:  ", pg);
	printBox(f, buf, page->getTrimBox());
	sprintf(buf, "Page %4d ArtBox:   ", pg);
	printBox(f, buf, page->getArtBox());
      }
    } else {
      page = doc->getCatalog()->getPage(firstPage);
      printBox(f, "MediaBox:       ", page->getMediaBox());
      printBox(f, "CropBox:        ", page->getCropBox());
      printBox(f, "BleedBox:       ", page->getBleedBox());
      printBox(f, "TrimBox:        ", page->getTrimBox());
      printBox(f, "ArtBox:         ", page->getArtBox());
    }
  }

  // print file size
#ifdef VMS
  sizef = fopen(fileName->getCString(), "rb", "ctx=stm");
#else
  sizef = fopen(fileName->getCString(), "rb");
#endif
  if (sizef) {
#if HAVE_FSEEKO
    fseeko(sizef, 0, SEEK_END);
    fprintf(f, "File size:      %u bytes\n", (Guint)ftello(sizef));
#elif HAVE_FSEEK64
    fseek64(sizef, 0, SEEK_END);
    fprintf(f, "File size:      %u bytes\n", (Guint)ftell64(sizef));
#else
    fseek(sizef, 0, SEEK_END);
    fprintf(f, "File size:      %d bytes\n", (int)ftell(sizef));
#endif
    fclose(sizef);
  }

  // print linearization info
  fprintf(f, "Optimized:      %s\n", doc->isLinearized() ? "yes" : "no");

  // print PDF version
  fprintf(f, "PDF version:    %.1f\n", doc->getPDFVersion());

  // print the metadata
  if (printMetadata && (metadata = doc->readMetadata())) {
    fputs("Metadata:\n", f);
    fputs(metadata->getCString(), f);
    fputc('\n', f);
    delete metadata;
  }
  
  if (f != stdout) {
    fclose(f);
  }
  
  exitCode = 0;
  
  // clean up
 err3:
  delete textFileName;
 err2:
  uMap->decRefCnt();
  delete doc;
 err1:
  delete globalParams;
 err0:

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return exitCode;
}

static void printInfoString(FILE *f, Dict *infoDict, char *key, char *text,
			    UnicodeMap *uMap) {
  Object obj;
  GString *s1;
  GBool isUnicode;
  Unicode u;
  char buf[8];
  int i, n;

  if (infoDict->lookup(key, &obj)->isString()) {
    fputs(text, f);
    s1 = obj.getString();
    if ((s1->getChar(0) & 0xff) == 0xfe &&
	(s1->getChar(1) & 0xff) == 0xff) {
      isUnicode = gTrue;
      i = 2;
    } else {
      isUnicode = gFalse;
      i = 0;
    }
    while (i < obj.getString()->getLength()) {
      if (isUnicode) {
	u = ((s1->getChar(i) & 0xff) << 8) |
	    (s1->getChar(i+1) & 0xff);
	i += 2;
      } else {
	u = pdfDocEncoding[s1->getChar(i) & 0xff];
	++i;
      }
      n = uMap->mapUnicode(u, buf, sizeof(buf));
      fwrite(buf, 1, n, f);
    }
    fputc('\n', f);
  }
  obj.free();
}

static void printInfoDate(FILE *f, Dict *infoDict, char *key, char *text) {
  Object obj;
  char *s;
  int year, mon, day, hour, min, sec, n;
  struct tm tmStruct;
  char buf[256];

  if (infoDict->lookup(key, &obj)->isString()) {
    fputs(text, f);
    s = obj.getString()->getCString();
    if (s[0] == 'D' && s[1] == ':') {
      s += 2;
    }
    if ((n = sscanf(s, "%4d%2d%2d%2d%2d%2d",
		    &year, &mon, &day, &hour, &min, &sec)) >= 1) {
      switch (n) {
      case 1: mon = 1;
      case 2: day = 1;
      case 3: hour = 0;
      case 4: min = 0;
      case 5: sec = 0;
      }
      tmStruct.tm_year = year - 1900;
      tmStruct.tm_mon = mon - 1;
      tmStruct.tm_mday = day;
      tmStruct.tm_hour = hour;
      tmStruct.tm_min = min;
      tmStruct.tm_sec = sec;
      tmStruct.tm_wday = -1;
      tmStruct.tm_yday = -1;
      tmStruct.tm_isdst = -1;
      // compute the tm_wday and tm_yday fields
      if (mktime(&tmStruct) != (time_t)-1 &&
	  strftime(buf, sizeof(buf), "%c", &tmStruct)) {
	fputs(buf, f);
      } else {
	fputs(s, f);
      }
    } else {
      fputs(s, f);
    }

    fputc('\n', f);
  }
  obj.free();
}

static void printBox(FILE *f, char *text, PDFRectangle *box) {
  fprintf(f, "%s%8.2f %8.2f %8.2f %8.2f\n",
	 text, box->x1, box->y1, box->x2, box->y2);
}
