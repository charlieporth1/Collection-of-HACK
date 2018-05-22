	var articleObj = null;
	var articleId = null;
	if (document.getElementById("cdsArticleObj")) {
		var articleStr = document.getElementById("cdsArticleObj").getAttribute("data-json");
		articleObj = JSON.parse(articleStr);
		articleId = articleObj.docId;
	}

	var headerStr = document.getElementById("cdsHeaderObj").getAttribute("data-json");
	var headerObj = JSON.parse(headerStr);

	function inputCleanupJS(textString) {
		if (textString == null) {
			return textString;
		}
		// fix for <rdar://problem/6055603> - remove all escapes before applying escapes (to avoid an attacker who is trying to escape the escapes)
		textString = textString.replace("\\", "");
		textString = textString.replace("\n", " ");
		textString = textString.replace("\r", " ");
		textString = textString.replace(/'/g, "\\\\'");
		textString = textString.replace("\"", "\\\"");
		return textString.trim();
	}

	if (articleObj != null && articleObj != undefined) {
		// Added as per the Radar ticket #9279075 Ends 
		if (articleObj.enableAffectedProductPortlet) {
			var baseLink = articleObj.baseLink;
			var lcl = articleObj.lcl;
			var documentId = articleObj.docId;
			var vLocale = articleObj.strLocale;
		}
		var ratingNoFlag = false;
		var ACStaticText = JSON.parse(articleObj.ACStaticText);

		if (articleObj.domainName == -1) {
			// this only needs to be set if we are in non-production (since KmLoader.js by default goes to km.support.apple.com)
			KmLoader.akamaiUrl = '';
			KmLoader.logCall = function (urlValue) {
				if (typeof console != 'undefined') console.log(urlValue);
			}
		} else {
			KmLoader.akamaiUrl = articleObj.akamaiUrl;
		}

		//function sendRead()
		var locale = articleObj.lcl;

		Event.observe(window, 'load', function () {

			ACHistory.addKbView(articleObj.docId, inputCleanupJS(articleObj.strTitle), articleObj.strLocale, articleObj.referringPage, articleObj.archive);
			var loadAddProd = articleObj.enableAffectedProductPortlet;
			var enablePortlet = articleObj.enableAffectedProductPortlet;

			sendImpression(articleObj.reportLink, articleObj.docId, articleObj.strLocale);
			try {
				if (articleObj.docId != null && articleObj.docId.toLowerCase().startsWith("ht")) {
					getNavTags(articleObj.docId, articleObj.publishedDate,articleObj.localeParam.toLowerCase().replace("_", "-"));
				}
			} catch (e) {}

			if (articleObj.configHeaderFooterLocale != null && articleObj.configHeaderFooterLocale.length > 0) {
				if (loadAddProd == "true" && enablePortlet == "true") {
					ACUtil.loadPSPUrl(baseLink, documentId, articleObj.configHeaderFooterLocale, vLocale, articleObj.strCategoryListRefKeys, articleObj.strChannel, articleObj.archive);
				} else {
					ACUtil.getMultipleOffers(articleObj.strCategoryListRefKeys, articleObj.strChannel, articleObj.configHeaderFooterLocale, articleObj.archive);
				}
			} else {
				if (loadAddProd == "true" && enablePortlet == "true") {
					ACUtil.loadPSPUrl(baseLink, documentId, lcl, vLocale, articleObj.strCategoryListRefKeys, articleObj.strChannel, articleObj.archive);
				} else {
					ACUtil.getMultipleOffers(articleObj.strCategoryListRefKeys, articleObj.strChannel, articleObj.lcl, articleObj.archive);
				}
			}

		});

	}
	document.observe("dom:loaded", function () {
		checkLanguageSelector();
		var StepSections = $$('div.steps');
		StepSections.each(function (el) {
			var images = el.select('p > img');
			images.each(function (node) {
				el.insert(node);
			});
			var empty = el.select('p:empty');
			empty.each(function (node) {
				node.remove();
			});
			var wrapper = new Element('div');
			el.insert({
				top: wrapper
			});
			var textContent = el.select('h2', 'p');
			textContent.each(function (node) {
				wrapper.insert(node);
			});
		});

		var StepWrapSections = $$('div.stepswrap');
		StepWrapSections.each(function (el) {
			var images = el.select('p > img');
			images.each(function (node) {
				el.insert(node);
			});
			var empty = el.select('p:empty');
			empty.each(function (node) {
				node.remove();
			});
			var wrapper = new Element('div');
			el.insert({
				top: wrapper
			});
			var textContent = el.select('h2', 'p');
			textContent.each(function (node) {
				wrapper.insert(node);
			});
		});

		// wrap all tables and make them responsive 
		var tables = $$('table');
		tables.each(function (el) {
			var div = new Element('div', {
				'class': 'table-responsive'
			});
			el.wrap(div);
		});

		// attach events to anchors for smooth scrolling 
		var anchorLinks = $$('a[href^="#"]');
		anchorLinks.each(function (el) {
			Element.observe(el, 'click', function (event) {
				Event.stop(event);
				var elementLink = el.readAttribute('href');
				var scrollHere = elementLink.substr(1, elementLink.length);
				Effect.ScrollTo(scrollHere);
				setTimeout(function () {
					$(scrollHere).writeAttribute("tabindex", -1).focus();
				}, 1000);
				return false;
			});
		});

		if (headerObj.setPod) {
			ACUtil.setPOD(headerObj.podValue);
		}

		var modelValue = headerObj.modelValue;
		var akamaiUrl = headerObj.akamaiUrl;
		var aiSearchServiceURL = headerObj.aiSearchServiceURL;
		var podLocaleValue = headerObj.podLocaleValue;
		var aiCrossDomainSearchServiceURL = headerObj.aiCrossDomainSearchServiceURL;
		var searchSection = 'support';
		var searchCountry = headerObj.searchCountry;
		var deactivateSearchShortcuts = true;
		var enableAppleInstant = headerObj.enableAppleInstant;

		if (articleObj != null) {
			if (articleObj.strChannel === "DOWNLOADS") {
				ACUtil.setDownloadsImage(articleObj.docId, articleObj.imgSrc, articleObj.akamaiUrl);
				var mainTitle = document.getElementById('main-title');
				if (articleObj.metaUrl != "null" && articleObj.metaUrl != undefined && articleObj.metaUrl != null && !(/^\s*$/.test(articleObj.metaUrl))) {
					var download_link = document.createElement('a');
					download_link.setAttribute('class', 'download-button');
					if (AC.Detector.isMobile() || AC.Detector.isiPhone() || AC.Detector.isiPad()) {
						download_link.setAttribute('href', "mailto:?Subject=" + articleObj.strTitle + "&Body=http://support.apple.com/kb/" + articleObj.docId + "?viewlocale=" + articleObj.strLocale);
						download_link.setAttribute('onclick', 'ACUtil.clickTracking("' + articleObj.omniturePageName + '::detailbuttontop")');
						var span_node = document.createElement('span');
						//error here	
						var text_node = document.createTextNode(articleObj.messageResourcesArticleTempEmail);
						download_link.appendChild(span_node);
						span_node.appendChild(text_node);
						mainTitle.parentNode.insertBefore(download_link, mainTitle.nextSibling);
					} else {
						download_link.setAttribute('href', articleObj.metaUrl);
						download_link.setAttribute('onclick', 'ACUtil.clickTracking("' + articleObj.omniturePageName + '::detailbuttontop")');
						var span_node = document.createElement('span');
						//error here	
						var text_node = document.createTextNode(articleObj.messageResourcesArticleTempDown);
						download_link.appendChild(span_node);
						span_node.appendChild(text_node);
						mainTitle.parentNode.insertBefore(download_link, mainTitle.nextSibling);
					}
				}
			}

			ACUtil.isOmnitureSupported = articleObj.isOmnitureSupported;
		}
	});

	function checkLanguageSelector() {
		var langViewLocale = articleObj == null ? "" : articleObj.viewLocale; //actual viewlocale parameter passed
		var localeParam = articleObj == null ? "" : articleObj.localeParam; //actual locale parameter passed.
		if ($$("#search_filters") != null && $$("#search_filters") != undefined && $$("#search_filters").length > 0) {
			if (langViewLocale != "null" && langViewLocale != "" && langViewLocale != null && langViewLocale != undefined) {
				if ($$("#search_filters option[value='" + langViewLocale + "']") != undefined && $$("#search_filters option[value='" + langViewLocale + "']")[0] != undefined) {
					$$("#search_filters option[value='" + langViewLocale + "']")[0].selected = true;
				} else { //if viewlocale is not there in lang drop down then select content locale
					$$("#search_filters option[value='" + locale + "']")[0].selected = true;
				}
			} else if (locale != "null" && locale != "" && locale != null && locale != undefined && $$("#search_filters option[value='" + locale + "']") != undefined && $$("#search_filters option[value='" + locale + "']")[0] != undefined) {
				if (localeParam != "null" && localeParam != "" && localeParam != null && localeParam != undefined && $$("#search_filters option[value='" + localeParam + "']") != undefined && $$("#search_filters option[value='" + localeParam + "']")[0] != undefined) {
					$$("#search_filters option[value='" + localeParam + "']")[0].selected = true;
				} else {
					$$("#search_filters option[value='" + locale + "']")[0].selected = true;
				}
			}
		}
	}
	window.onpopstate = function (event) {
		if (AC.Detector.isiPad()) {
			checkLanguageSelector();
		}
	}


	/**
	 * This file is for including common functions being used
	 * for article details page to remove multiple request being fired
	 * @author aastha
	 * @date 22/09/15
	 */

	/* Start : form validation for Ask other users about this article */

	function submitQuestionToForum(a, d, c, e) {
		var b = $("interactiveQuestionSearchField").value;
		if (b.blank() || b == "" || b == e) {
			document.getElementById("interactiveQuestionSearchField").focus();
			alert(c);
			return false
		} else {
			var f = $("question-form");
			f.action = a + "articleId=" + d + "&articleQuestion=" +
				encodeURIComponent(b);
			f.submit()
		}
	}

	/* End : form validation for Ask other users about this article */

	/* Start : Rating implementation for helpful and not helpful buttons */

	function rateNotHelpful(buttonClicked, commentBoxChannelList, strChannel,
		docId, locale) {
		if (commentBoxChannelList.indexOf(strChannel) != -1) {
			ACRating.submitCall(buttonClicked, 1, docId, locale, '');
			$('question-state').hide();
			$('feedback-state').show();
			ACRating.documentId = docId;
			ACRating.locale = locale;
		} else { // ratings submission for non HT/TS articles 
			ACRating.submitCall(buttonClicked, 1, docId, locale, "");
		}
		$("feedback").focus(); //writeAttribute("tabindex",-1).focus();
		$($$("textarea#feedback")[0]).observe('paste', function (event) {
			limitText();
		});
	}
	// function for submission of ratings on the click of Submit and Cancel from the feedback menu.
	function feedbackSubmit(buttonClicked, docId, locale) {
		var comments = $('feedback').value;

		$('feedback-state').hide();
		if (buttonClicked == 'Submit') {
			comments = comments.trim();
			if (comments == '') {
				ACRating.hideCommentsBox(buttonClicked);
			} else {
				ACRating.submitCall(buttonClicked, 1, docId, locale, comments);
			}
		} else if (buttonClicked == 'Cancel') {
			ACRating.hideCommentsBox(buttonClicked);
		}
	}

	function limitText() {
		var textareaValue = document.getElementById("feedback").value;
		if (textareaValue.length > 1024) {
			textareaValue = textareaValue.substr(0, 1024);
			document.getElementById("feedback").value = textareaValue;
		}
	}
	/* End : Rating implementation for helpful and not helpful buttons */


	/* Start : ACRating.js creation of object */

	var ACRating = {
		'submitRating': function (buttonClicked, ratingNum, id, locale, comments, visitorId) {
			var xmlhttp;
			var currentUri = ACUtil.getBaseURL();
			ACRating.articleId = id;
			$('question-state').style.display = "none";
			$('rating-send').style.display = "block";
			comments = encodeURI(comments, "UTF-8");
			var url = currentUri + "kb/index?page=ratingData&rating=" + ratingNum + "&id=" + id + "&locale=" + locale + "&comments=" + comments + "&visitorId=" + visitorId;
			// xml http request for submitting the ratings
			if (window.XMLHttpRequest) {
				xmlhttp = new XMLHttpRequest();
			} else {
				xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
			}
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {}
			}
			xmlhttp.open("GET", url, false);
			xmlhttp.send();
			// timeout for aborting the synchronous request if response is not coming
			var xmlHttpTimeout = setTimeout(ajaxTimeout, 5000);

			function ajaxTimeout() {
				xmlhttp.abort();
			}

			ACRating.hideCommentsBox(buttonClicked);

			return true;
		},

		'submitCall': function (buttonClicked, ratingNum, id, locale, comments) {
			var ratingResponse = "";
			var visitorId = "";
			ACRating.submitOmniture(buttonClicked, ratingNum);
			try {
				ratingResponse = ACRating.submitRating(buttonClicked, ratingNum, id, locale, comments, visitorId);
			} catch (e) {}
			return ratingResponse;
		},

		'submitOmniture': function (buttonClicked, ratingNum) {
			try {
				if (buttonClicked == 'Yes' || buttonClicked == 'No') {
					var helpful = ratingNum == 5 ? 'yes' : 'no';
					var s = s_gi(s_account);
					s.eVar2 = 'acs::kb::article rating::helpful=' + helpful;
					s.events = 'event2';
					void(s.t());
				}
			} catch (e) {}
		},

		'hideCommentsBox': function (buttonClicked) {
			// Hiding the feedback menu and displaying the feedback response message
			//$('rating-send').style.visibility = "hidden";
			$('rating-send').style.display = "none";

			if (ACRating.articleId.indexOf('HT') != -1 || ACRating.articleId.indexOf('TS') != -1 || ACRating.articleId.indexOf('KM') != -1) {
				if (buttonClicked == 'Yes' || buttonClicked == 'Submit' || buttonClicked == 'Cancel') {
					$('rating-done').style.display = "block";
					$('results-helpful').style.height = '0px';
				}
			} else {
				$('rating-done').style.display = "block";
			}
			$('rating-done').writeAttribute("tabindex", -1).focus();
			$('results-helpful').style.visibility = "hidden";
			$('feedback').value = "";
		}
	};
	/* End : ACRating.js creation of object */

	/* Start : function required for language drop down change */

	function selectLanguage(selectedLocale, localeParamPassed) {
		if (localeParamPassed != undefined && localeParamPassed != undefined && localeParamPassed != "" && localeParamPassed != "null" && localeParamPassed != "NULL" && localeParamPassed != null) {
			window.location = '/kb/' + articleId + '?viewlocale=' + selectedLocale + '&locale=' + localeParamPassed;
		} else {
			window.location = '/kb/' + articleId + '?viewlocale=' + selectedLocale;
		}
	}
	/* End : function required for language drop down change */


	function sendImpression(reportLink, documentId, locale) {
		return new Promise(function (resolve, reject) {
			var req = new XMLHttpRequest();
			var url = reportLink + '/kb/index?page=impression&docid=' + documentId + '&locale=' + locale;

			req.open('GET', url);
			req.setRequestHeader('Content-Type', 'application/json');
			req.onload = function () {
				if (req.status == 200) {

					if (req.response == undefined) {
						// nothing to be done here
					} else {
						ACUtil.processImpressionResponse(req.response);
					}

				}
			}

			// Handle network errors
			req.onerror = function () {
				// reject(Error("Network Error"));
			};

			// Make the request
			req.send();

		});
	}



	function getNavTags(articleId, publishedDate, locale) {
		return new Promise(function (resolve, reject) {
			var req = new XMLHttpRequest();
			var url = "/ols/api/article/" + locale + "/" + articleId;

			req.open('GET', url);
			req.setRequestHeader('Content-Type', 'application/json');
			if(publishedDate!=null){
				req.setRequestHeader('If-Modified-Since', publishedDate);
			}
			req.onload = function () {
				if (req.status == 200) {

					if (req.response == undefined) {
						// nothing to be done here
					} else {
						var tags = parseNavTags(JSON.parse(req.response).article.categoryInfo);
						updateNavTags(tags, locale);

					}

				}
			}

			// Handle network errors
			req.onerror = function () {
				// reject(Error("Network Error"));
			};

			// Make the request
			req.send();

		});
	}

	function parseNavTags(categoryInfo) {
		var categoryJson = [];
		var tags = [];

		var categories = categoryInfo.categories;
		var categoryDictionaries = categoryInfo.categoryDictionaries;

		categoryDictionaries.forEach(function (category) {
			categoryJson[category.key] = category;
		});

		var extractNavTagCategory = category => {

			let categoryKey = category.split("/").pop();
			if (parseInt(categoryJson[categoryKey].articleCount) > 2) {
				var name = categoryJson[categoryKey].name;
				var tagId = categoryJson[categoryKey].key;
				var showTagOnSS = categoryJson[categoryKey].meta ? categoryJson[categoryKey].meta.customerDisplayable[0] : true; 
				if (showTagOnSS && name.trim() != '' && (name.toLowerCase() != tagId.toLowerCase())) {
					tags.push(categoryJson[categoryKey]);
				}

			}

		}

		// iterate all the tags that article is mapped with
		categories.map(function (category) {
			if (category.indexOf("TAX_NavigationTax") != -1) {
				extractNavTagCategory(category);
			}
		})

		return tags;

	}

	function updateNavTags(tags, locale) {
		if (document.getElementById('showNavTagFlag').value == "true") {
			var tagLocalisedString = document.getElementById('tags-localised').value;

			var allTags = tags.map((tag, index) => {
				var tagNameInURL = tag.englishName.toLowerCase();
        		tagNameInURL = tagNameInURL.replace(/\s+/g, ' ');
        		tagNameInURL = tagNameInURL.replace(/\s/g, '-');
				return `<div class="book-topic-tags"><a href="/${locale}/tags/${tagNameInURL}">${tag.name}<span class="a11y">${tagLocalisedString}</span></a></div>`
			});
			var h2value = document.getElementById('mimosa-tags-heading').value;
			var navTagParentElement = `<div id="navTags" class="book-related-topics"><h2 class="book-related-topics-eyebrow">${h2value}</h2>`

			document.getElementById('mimosa-tags').innerHTML = "";
			if (allTags.size() > 0) {
				document.getElementById('mimosa-tags').innerHTML = navTagParentElement + allTags.join(" ") + "</div>";
				try {
					if (_analytics && _analytics.scrapingFunctions && typeof _analytics.scrapingFunctions.assignAnalyticsEvents === "function") {
						_analytics.scrapingFunctions.assignAnalyticsEvents()
					}
				} catch (e) {}
			}
		}

	}