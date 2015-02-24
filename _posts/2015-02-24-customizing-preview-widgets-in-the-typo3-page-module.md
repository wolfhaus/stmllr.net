---
layout: post
title: Customizing preview widgets in the TYPO3 page module
date: 2015-02-24 00:31
excerpt: The user experience of the TYPO3 page module suffers as soon as you insert content elements with custom fields. The preview widgets all look like the same, hiding important information. How to solve this?
tags:
  - TYPO3
  - Backend
---

### Motivation

As a developer for the Content Management System TYPO3, you usually customize and extend the system
for the particular use case of your stake holders. Giving their editors the best user experience
is a means to achieve acceptance and performance of managing content with TYPO3.

In the page module there are preview widgets for all content elements on the page.
The preview shows the header plus the value of the most important field for all default content elements.
For example, the image is the most important property for content elements of type "image",
therefore it is rendered in the preview, together with the header:

![cType image preview](/files/images/ctype-image.png)

When you create custom content elements, you have to tell TYPO3 how to render the preview widget.
By default only the standard fields from the core are rendered, depending on the "types" definition in TCA.
If you have custom fields which store the important information, the preview widget might look quite unintuitive.

![cType image preview](/files/images/ctype-person-poor.png)

Get this issue solved by hooking into the rendering process of the preview widget.
Unfortunately there is no clean-and-simple-API for that, but you can solve it with a hook and some PHP magic.

Let's use the [modelling by example](http://everzet.com/post/99045129766/introducing-modelling-by-example)
approach from [Behavior Driven Development](http://dannorth.net/introducing-bdd/) to describe our
feature:

{% highlight gherkin %}
Feature: Preview widgets in page module showing custom fields of content elements
  As a TYPO3 editor
  I want to see important properties of content elements in the preview of the page module
  In order to have a better user experience

  Background:
    Given you have a TYPO3 extension "demo"
    And the extension ships a custom content element of type "person"
    And the content element has a custom field "name" of type "text"
    And the content element has a custom field "portrait" of type "image"

  Scenario: Show the name and the portrait of the content element "person" in the page module
    Given a page "staff" was added to the pagetree
    And a content element "CEO" of type "person" was added to "staff"
    And the name "John Doe" was added to the field "name" of "CEO"
    And a picture was added to the field "portrait" of "CEO"
    When you go to the page module
    And you select "staff" as the current page
    Then you should see "John Doe" in the preview widget of "person"
    And you should see a thumbnail image from "portrait" in the preview widget of "person"
{% endhighlight %}

Much better if it would look like this:

![cType image preview](/files/images/ctype-person-rich.png)

### Solution

The rendering of the preview can be found in `\TYPO3\CMS\Backend\View\PageLayoutView`.
It provides a hook for customization. Your custom rendering definition has to implement the
interface `\TYPO3\CMS\Backend\View\PageLayoutViewDrawItemHookInterface`.

Here's the example code:

EXT:demo/ext_localconf.php

{% highlight php startinline %}
$GLOBALS['TYPO3_CONF_VARS']['SC_OPTIONS']['cms/layout/class.tx_cms_layout.php']['tt_content_drawItem']['mydemo'] =
	'Vendor\\Demo\\Hooks\\CustomPageLayoutView';
{% endhighlight %}

EXT:demo/Classes/Hooks/CustomPageLayoutView.php
{% highlight php %}
<?php
namespace Vendor\Demo\Hooks;

/**
 * LICENSE: GPL2+
 * mainly derived from EXT:news by Georg Ringer et al.
 */
use TYPO3\CMS\Backend\View\PageLayoutViewDrawItemHookInterface,
    TYPO3\CMS\Backend\View\PageLayoutView;
/**
 * Hook to render preview widget of custom content elements in page module
 * @see \TYPO3\CMS\Backend\View\PageLayoutView::tt_content_drawItem()
 */
class CustomPageLayoutView implements PageLayoutViewDrawItemHookInterface {

	/**
	 * Preprocesses the preview rendering of a content element.
	 *
	 * @param PageLayoutView $parentObject Calling parent object
	 * @param boolean $drawItem Whether to draw the item using the default functionalities
	 * @param string $headerContent Header content
	 * @param string $itemContent Item content
	 * @param array $row Record row of tt_content
	 * @return void
	 */
	public function preProcess(&$parentObject, &$drawItem, &$header, &$item, &$row) {
		if ($row['CType'] !== 'demo_person') return;

		$drawItem = FALSE;
		$header = '<strong>' . htmlspecialchars($row['header']) . '</strong><br />';
		$item = htmlspecialchars($row['tx_demo_name']) . '<br><br>';
		$item .= $parentObject->thumbCode($row, 'tt_content', 'tx_demo_portrait');
	}
}
{% endhighlight %}

A more detailed example can be found in [EXT:news](https://github.com/TYPO3-extensions/news/blob/master/Classes/Hooks/CmsLayout.php).
