#include "chat_multiplayer.h"
#include <memory>
#include <vector>
#include <utility>
#include <regex>
#include "window_base.h"
#include "scene.h"
#include "bitmap.h"
#include "output.h"
#include "drawable_mgr.h"
#include "font.h"
#include "cache.h"
#include "input.h"
#include "utils.h"

extern "C" void SendChatMessageToServer(const char* msg);
extern "C" void ChangeName(const char* name);
extern std::string multiplayer__my_name;

class ChatBox : public Drawable {
	struct Message {
		BitmapRef renderGraphic;
		std::string playerName;
		std::string message;
		bool dirty = false; // need to redraw? (for when UI skin changes)
	};

	//design parameters
	const unsigned int panelBleed = 16; // how much to stretch top, bottom and right edges of panel offscreen (so only left frame shows)
	const unsigned int panelFrame = 4; // width of panel's visual frame (on left side)
	const unsigned int typeHeight = 24; // height of type box
	const unsigned int typePaddingHorz = 8; // padding between type box edges and content (left)
	const unsigned int typePaddingVert = 6; // padding between type box edges and content (top)
	const unsigned int messageMargin = 3; // horizontal margin between panel edges and content
	const unsigned int namePromptMargin = 40; // left margin for type box to make space for "name:" prompt
	
	Window_Base backPanel; // background pane
	Window_Base scrollBox; // box used as rendered design for a scrollbar
	std::vector<Message> messages;
	unsigned int scrollPosition = 0;
	unsigned int scrollContentHeight = 0; // height of scrollable message log (including offscren)
	bool focused = false;
	BitmapRef typeText;
	std::vector<unsigned int> typeCharOffsets; // cumulative x offsets for each character in the type box. Used for rendering the caret
	BitmapRef typeCaret;
	unsigned int typeCaretIndex = 0;
	unsigned int typeScroll = 0; // horizontal scrolling of type box
	bool nameInputEnabled = true;
	BitmapRef namePrompt; // graphic used for the "name:" prompt

	void updateScrollBar() {
		const unsigned int logVisibleHeight = (focused) ?
			SCREEN_TARGET_HEIGHT-typeHeight
		:	SCREEN_TARGET_HEIGHT;
		if(scrollContentHeight <= logVisibleHeight) {
			scrollBox.SetX(TOTAL_TARGET_WIDTH); // hide scrollbar if content isn't large enough for scroll
			return;
		}
		scrollBox.SetX(TOTAL_TARGET_WIDTH-panelFrame); // show scrollbar
		// position scrollbar
		const float ratio = logVisibleHeight/float(scrollContentHeight);
		const unsigned int barHeight = logVisibleHeight*ratio;
		const unsigned int barY = scrollPosition*ratio;
		scrollBox.SetHeight(barHeight);
		scrollBox.SetY(logVisibleHeight-barY-barHeight);
	}

	void updateTypeBox() {
		if(!focused) {
			backPanel.SetCursorRect(Rect(0, 0, 0, 0));
		} else {
			const unsigned int f = -8; // SetCursorRect for some reason already has a padding of 8px relative to the window, so we fix it
			const unsigned int namePad = nameInputEnabled ? namePromptMargin : 0;
			backPanel.SetCursorRect(Rect(f+panelFrame+namePad, f+panelBleed+SCREEN_TARGET_HEIGHT-typeHeight, CHAT_TARGET_WIDTH-panelFrame-namePad, typeHeight));
		}
	}

	BitmapRef buildMessageGraphic(std::string playerName, std::string msg, unsigned int& graphicHeight) {
		std::string leadingColorString = "";
		if(playerName.size() > 0) leadingColorString = playerName+": ";
		// manual text wrapping
		const unsigned int maxWidth = CHAT_TARGET_WIDTH-panelFrame*2-messageMargin*2;
		std::vector<std::pair<std::string, unsigned int>> lines; // individual lines saved so far, along with their y offset
		unsigned int totalWidth = 0; // maximum width between all lines
		unsigned int totalHeight = 0; // accumulated height from all lines

		std::string currentLine = leadingColorString+msg; // current line being worked on. start with whole message.
		std::string nextLine = ""; // stores characters moved down to line below
		do {
			auto rect = Font::Tiny()->GetSize(currentLine);
			while(rect.width > maxWidth) {
				// as long as current line exceeds maximum width,
				// move one character from this line down to the next one
				nextLine = currentLine.back()+nextLine;
				currentLine.pop_back();
				// recalculate line width with that character having been moved down
				rect = Font::Tiny()->GetSize(currentLine);
			}
			// once line fits, save it
			lines.push_back(std::make_pair(currentLine, totalHeight));
			totalWidth = std::max<unsigned int>(totalWidth, rect.width);
			totalHeight += rect.height;
			// repeat this work on the exceeding portion moved down to the line below
			currentLine = nextLine;
			nextLine = "";
		} while(currentLine.size() > 0);

		// once all lines have been saved
		// render them into a bitmap
		BitmapRef text_img = Bitmap::Create(totalWidth+1, totalHeight+1, true);
		unsigned int colorCharacters = leadingColorString.size(); // remaining colored characters to draw
		int nLines = lines.size();
		for(int i = 0; i < nLines; i++) {
			auto& line = lines[i];
			unsigned int nonColoredStartPos = 0; // starting position to draw non-colored text. Non zero if colored text has been drawn
			unsigned int lineCChar = std::min<unsigned int>(colorCharacters, line.first.size()); // how many colored characters to draw on this line
			colorCharacters -= lineCChar; // decrement total colored characters remaining for next line
			if(lineCChar > 0) {
				auto cRect = Text::Draw(*text_img, 0, line.second, *Font::Tiny(), *Cache::SystemOrBlack(), 1, line.first.substr(0, lineCChar)); // colored text if necessary
				nonColoredStartPos = cRect.width;
			}
			Text::Draw(*text_img, nonColoredStartPos, line.second, *Font::Tiny(), *Cache::SystemOrBlack(), 0, line.first.substr(lineCChar, std::string::npos)); // non colored text
		}
		// return
		graphicHeight = totalHeight;
		return text_img;
	}
public:
	ChatBox() : Drawable(2106632960, Drawable::Flags::Global),
				// stretch 16px at right, top and bottom sides so the edge frame only shows on left
				backPanel(SCREEN_TARGET_WIDTH, -panelBleed, CHAT_TARGET_WIDTH+panelBleed, SCREEN_TARGET_HEIGHT+panelBleed*2, Drawable::Flags::Global),
				scrollBox(0, 0, panelFrame+panelBleed, 0, Drawable::Flags::Global)
		{
		DrawableMgr::Register(this);

		backPanel.SetContents(Bitmap::Create(CHAT_TARGET_WIDTH, SCREEN_TARGET_HEIGHT));
		backPanel.SetZ(2106632959);

		scrollBox.SetZ(2106632960);
		scrollBox.SetVisible(false);

		// create caret (type cursor) graphic
		std::string caretChar = "｜";
		const unsigned int caretLeftKerning = 6;
		auto cRect = Font::Default()->GetSize(caretChar);
		typeCaret = Bitmap::Create(cRect.width+1-caretLeftKerning, cRect.height+1, true);
		Text::Draw(*typeCaret, -caretLeftKerning, 0, *Font::Default(), *Cache::SystemOrBlack(), 0, caretChar);

		// create "name:" prompt graphic
		std::string prompt = "Name:";
		auto nRect = Font::Default()->GetSize(prompt);
		namePrompt = Bitmap::Create(nRect.width+1, nRect.height+1, true);
		Text::Draw(*namePrompt, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 1, prompt);
	}

	void Draw(Bitmap& dst) {
		/*
			draw chat log
		*/
		// how much of the log is visible. Depends on whether or not the type box is active
		const unsigned int logVisibleHeight = (focused) ?
			SCREEN_TARGET_HEIGHT-typeHeight
		:	SCREEN_TARGET_HEIGHT;
		int nextHeight = -scrollPosition; // y offset to draw next message, from bottom of log panel
		unsigned int nMessages = messages.size();
		for(int i = nMessages-1; i >= 0; i--) {
			BitmapRef mb = messages[i].renderGraphic;
			auto rect = mb->GetRect();
			nextHeight += rect.height;
			if(nextHeight <= 0) continue; // skip drawing offscreen messages, but still accumulate y offset (bottom offscreen)
			Rect cutoffRect = Rect(rect.x, rect.y, rect.width, std::min<unsigned int>(rect.height, nextHeight)); // don't let log be drawn over type box region
			if(messages[i].dirty) {
				unsigned int dummyRef;
				mb = messages[i].renderGraphic = buildMessageGraphic(messages[i].playerName, messages[i].message, dummyRef); // redraw message graphic if needed, and if visible
				messages[i].dirty = false;
			}
			dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+messageMargin, logVisibleHeight-nextHeight, *mb, cutoffRect, Opacity::Opaque());
			if(nextHeight > logVisibleHeight) break; // stop drawing offscreen messages (top offscreen)
		}
		/*
			draw type text
		*/
		if(focused) {
			const unsigned int namePad = nameInputEnabled ? namePromptMargin : 0;
			const unsigned int typeVisibleWidth = CHAT_TARGET_WIDTH-panelFrame-typePaddingHorz*2-namePad;
			auto rect = typeText->GetRect();
			Rect cutoffRect = Rect(typeScroll, rect.y, std::min<int>(typeVisibleWidth, rect.width-typeScroll), rect.height); // crop type text to stay within padding
			dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+namePad+typePaddingHorz, SCREEN_TARGET_HEIGHT-typeHeight+typePaddingVert, *typeText, cutoffRect, Opacity::Opaque());

			// draw caret
			dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+namePad+typePaddingHorz+typeCharOffsets[typeCaretIndex]-typeScroll, SCREEN_TARGET_HEIGHT-typeHeight+typePaddingVert, *typeCaret, typeCaret->GetRect(), Opacity::Opaque());

			// draw name prompt if necessary
			if(nameInputEnabled) {
				dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+typePaddingHorz, SCREEN_TARGET_HEIGHT-typeHeight+typePaddingVert, *namePrompt, namePrompt->GetRect(), Opacity::Opaque());
			}
		}
	}

	void updateTypeText(std::u32string t) {
		// get char offsets for each character in type box, for caret positioning
		typeCharOffsets.clear();
		Rect accumulatedRect;
		const unsigned int nChars = t.size();
		for(int i = 0; i <= nChars; i++) {
			// for every substring of sizes 0 to N, inclusive, starting at the left
			std::u32string textSoFar = t.substr(0, i);
			accumulatedRect = Font::Default()->GetSize(Utils::EncodeUTF(textSoFar)); // absolute offset of character at this point (considering kerning of all previous ones)
			typeCharOffsets.push_back(accumulatedRect.width);
		}

		// final value assigned to accumulatedRect is whole type string
		// create Bitmap graphic for text
		typeText = Bitmap::Create(accumulatedRect.width+1, accumulatedRect.height+1, true);
		Text::Draw(*typeText, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 0, Utils::EncodeUTF(t));
	}

	void appendMessage(std::string playerName, std::string msg) {
		unsigned int graphicHeight;
		BitmapRef text_img = buildMessageGraphic(playerName, msg, graphicHeight);
		// append message
		Message _m = {text_img, playerName, msg};
		messages.push_back(_m);
		scrollContentHeight += (graphicHeight+1);
		updateScrollBar();
	}

	void scrollUp() {
		// scroll up and assert bounds
		const unsigned int logVisibleHeight = (focused) ?
			SCREEN_TARGET_HEIGHT-typeHeight
		:	SCREEN_TARGET_HEIGHT;
		const unsigned int maxScroll = std::max<int>(0, scrollContentHeight-logVisibleHeight); // maximum value for scrollPosition (how much height escapes from top)
		scrollPosition += 4;
		scrollPosition = std::min<unsigned int>(scrollPosition, maxScroll);
		updateScrollBar();
	}

	void scrollDown() {
		// scroll down and assert bounds
		scrollPosition -= std::min<int>(4, scrollPosition); // minimum value for scrollPosition is 0
		updateScrollBar();
	}

	void seekCaret(unsigned int i) {
		typeCaretIndex = i;
		// adjust type box horizontal scrolling based on caret position (always keep it in-bounds)
		const unsigned int namePad = nameInputEnabled ? namePromptMargin : 0;
		const unsigned int typeVisibleWidth = CHAT_TARGET_WIDTH-panelFrame-typePaddingHorz*2-namePad;
		const unsigned int caretOffset = typeCharOffsets[typeCaretIndex]; // absolute offset of caret in relation to type text contents
		const int relativeOffset = caretOffset-typeScroll; // caret's position relative to viewable portion of type box
		if(relativeOffset < 0) {
			// caret escapes from left side. adjust
			typeScroll += relativeOffset;
		} else if(relativeOffset >= typeVisibleWidth) {
			// caret escapes from right side. adjust
			typeScroll += relativeOffset-typeVisibleWidth;
		}
	}

	void onFocus() {
		focused = true;
		updateTypeBox();
		//enable scrollbar
		scrollBox.SetVisible(true);
		updateScrollBar();
	}

	void onUnfocus() {
		focused = false;
		updateTypeBox();
		//disable scrollbar
		scrollBox.SetVisible(false);
		//reset scroll
		scrollPosition = 0;
	}

	void updateSkin() {
		auto skin = Cache::SystemOrBlack();
		backPanel.SetWindowskin(skin);
		scrollBox.SetWindowskin(skin);
		const unsigned int nMsgs = messages.size();
		for(int i = 0; i < nMsgs; i++) {
			messages[i].dirty = true; // all messages now need to be redrawn with different UI skin
		}
	}

	void showNameInput(bool n) {
		nameInputEnabled = n;
		updateTypeBox();
	}
};

static const unsigned int MAXCHARSINPUT_NAME = 7;
static const unsigned int MAXCHARSINPUT_MESSAGE = 200;

static std::unique_ptr<ChatBox> chatBox; //chat renderer
static std::u32string typeText;
static unsigned int typeCaretIndex = 0;
static unsigned int maxChars = MAXCHARSINPUT_NAME;

static void createChatWindow(std::shared_ptr<Scene>& scene_map) {
	//select map's scene as current
	auto old_list = &DrawableMgr::GetLocalList();
	DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
	//append new chat window (added to current scene via constructor)
	chatBox = std::make_unique<ChatBox>();
	chatBox->appendMessage("", "[TAB]: focus/unfocus chat.");
	chatBox->appendMessage("", "[↑, ↓]: scroll chat.");
	chatBox->appendMessage("", "You must set a nickname");
	chatBox->appendMessage("", "  before you can chat.");
	chatBox->appendMessage("", "Max 7 characters. Enter numbers");
	chatBox->appendMessage("", "  and latin letters only.");
	chatBox->appendMessage("", "=====");
	//restore previous list
	DrawableMgr::SetLocalList(old_list);
}

void Chat_Multiplayer::tryCreateChatWindow() { // initialize if haven't already. Invoked by game_multiplayer.cpp
	//create if window not created and map already loaded (i.e. first map to load)
	if(chatBox.get() == nullptr) {
		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if(scene_map == nullptr) {
			Output::Debug("unexpected");
		} else {
			createChatWindow(scene_map);
		}
	} else {
		chatBox->updateSkin(); // refresh skin to new cached one
	}
}

void Chat_Multiplayer::gotMessage(std::string msg) {
	if(chatBox.get() == nullptr) return; // chatbox not initialized yet
	// TODO: currently extracting playername from message manually.
	// Have server send a separate field for message sender's name.
	std::string playerName = "";
	if(msg[0] == '<') {
		const unsigned int pos = msg.find('>');
		playerName = msg.substr(1, pos-1);
		msg = msg.substr(pos+2, std::string::npos);
	}
	chatBox->appendMessage(playerName, msg);
}

void Chat_Multiplayer::focus() {
	if(chatBox.get() != nullptr) {
		chatBox->onFocus();
	} else {
		Input::setGameFocus(true);
	}
}

static void unfocus() {
	if(chatBox.get() != nullptr) chatBox->onUnfocus();
	Input::setGameFocus(true);
}

void Chat_Multiplayer::processInputs() {
	if(Input::IsExternalTriggered(Input::InputButton::CHAT_UNFOCUS)) {
		unfocus();
	} else {
		if(chatBox.get() == nullptr) return; // chatbox not initialized yet
		/*
			scroll
		*/
		if(Input::IsExternalPressed(Input::InputButton::CHAT_UP)) {
			chatBox->scrollUp();
		}
		if(Input::IsExternalPressed(Input::InputButton::CHAT_DOWN)) {
			chatBox->scrollDown();
		}
		/*
			typing
		*/
		// input
		std::string inputText = Input::getExternalTextInput();
		if(inputText.size() > 0) {
			std::u32string inputU32 = Utils::DecodeUTF32(inputText);
			std::u32string fits = inputU32.substr(0, maxChars-typeText.size());
			typeText.insert(typeCaretIndex, fits);
			typeCaretIndex += fits.size();
		}
		// erase
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_DEL_BACKWARD)) {
			if(typeCaretIndex > 0) typeText.erase(--typeCaretIndex, 1);
		}
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_DEL_FORWARD)) {
			typeText.erase(typeCaretIndex, 1);
		}
		// move caret
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_LEFT)) {
			if(typeCaretIndex > 0) typeCaretIndex--;
		}
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_RIGHT)) {
			if(typeCaretIndex < typeText.size()) typeCaretIndex++;
		}
		// update type box
		chatBox->updateTypeText(typeText);
		chatBox->seekCaret(typeCaretIndex);
		/*
			send
		*/
		if(Input::IsExternalTriggered(Input::InputButton::CHAT_SEND)) {
			if(multiplayer__my_name == "") { // name not set, type box should send name
				// validate name. 
				// TODO: Server also validates name, but client should receive confirmation from it
				// instead of performing an equal validation
				std::string utf8text = Utils::EncodeUTF(typeText);
				std::regex reg("^[A-Za-z0-9]+$");
				if(	typeText.size() > 0 &&
					typeText.size() <= 7 &&
					std::regex_match(utf8text, reg)	) {
					// name valid, send
					ChangeName(utf8text.c_str());
					// reset typebox
					typeText.clear();
					typeCaretIndex = 0;
					chatBox->updateTypeText(typeText);
					chatBox->seekCaret(typeCaretIndex);
					//unfocus();
					// change chatbox state to allow for chat
					chatBox->showNameInput(false);
					maxChars = MAXCHARSINPUT_MESSAGE;
				}
			} else { // else it's used for sending messages
				SendChatMessageToServer(Utils::EncodeUTF(typeText).c_str());
				// reset typebox
				typeText.clear();
				typeCaretIndex = 0;
				chatBox->updateTypeText(typeText);
				chatBox->seekCaret(typeCaretIndex);
				//unfocus();
			}
		}
	}
}