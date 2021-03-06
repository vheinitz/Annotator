
#include <vector>
#include <iostream>
#include <string>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;


std::string outbasepath;
std::string imgbasepath;// = "c:/Development/EPT_2018_04_09/EPT3/img/";
std::string annfile;// = "c:/Development/EPT_2018_04_09/EPT3/ann.txt";

//Cmd line params: --base=c:/Development/EPT_2018_04_09/EPT3/img  --ann=c:/Development/EPT_2018_04_09/EPT3/ann.txt

RNG rng(222);

class Annotator
{
public:
	Annotator()
	{
	}
	std::vector<std::string> _images;
	std::map<int, cv::Scalar> _labelColors;
	int _curImageIdx=0;
	std::string _curImage;
	int _curLabel=0;
	int _scale = 2;
	struct ann_point { cv::Point _p; int _edge; int _l; ann_point(cv::Point p = cv::Point(0, 0), int edge=20, int l = -1) :_p(p),_edge(edge), _l(l) {} };
	std::map<std::string, std::list<ann_point> > _pointAnnotation;

	const std::list<ann_point> & annotations() const
	{
		return annotations(_curImage);
	}

	const std::list<ann_point> & annotations( const std::string & img ) const
	{
		std::map<std::string, std::list<ann_point> >::const_iterator it = _pointAnnotation.find(img);

		if ( it == _pointAnnotation.end() )
		{
			return it->second;
		}

		throw std::out_of_range( "no image in map" );
	}

	void addPoint(cv::Point p, int edge )
	{
		_pointAnnotation[_curImage].push_back(ann_point(p, edge, _curLabel));
	}

	void removePoint(cv::Point p, int edge)
	{
		cv::Rect r = cv::Rect(cv::Point(p.x - edge , p.y - edge ), cv::Point(p.x + edge , p.y + edge ));
		std::list<ann_point> tmp;
		for (auto i : _pointAnnotation[_curImage])
		{
			if (!r.contains(i._p)) tmp.push_back(i);
		}
		_pointAnnotation[_curImage] = tmp;
	}

	void addPoint( cv::Point p, int edge, int l )
	{
		_pointAnnotation[_curImage].push_back(ann_point(p, edge, l));
	}

	void addPoint(const std::string & img, cv::Point p, int edge, int l)
	{
		addImage(img);
		_pointAnnotation[img].push_back(ann_point(p, edge, l));
	}

	void addImage(const std::string & img)
	{
		if (_pointAnnotation.find(img) == _pointAnnotation.end())
		{
			if (_images.size() == 0)
				_curImage = img;

			_images.push_back(img);
			_pointAnnotation[img];
		}
	}

	const std::string & curImg() const 
	{
		return _curImage;
	}

	int curLabel() const
	{
		return _curLabel;
	}

	cv::Scalar curLabelColor()
	{
		return _labelColors[ _curLabel ];
	}

	int nextLabel()
	{
		std::map<int, cv::Scalar>::iterator it =  _labelColors.find( _curLabel );
		if (++it == _labelColors.end())
		{
			it = _labelColors.begin();
		}
		_curLabel = it->first;
		return _curLabel;
	}

	int firstLabel()
	{
		std::map<int, cv::Scalar>::iterator it = _labelColors.begin();
		_curLabel = it->first;
		return _curLabel;
	}

	void addLabel(int l)
	{
		_curLabel = l;
		_labelColors[l] = Scalar( rng.uniform(50, 255), rng.uniform(50, 255), rng.uniform(50, 255) );
	}

	void addLabel(int l, cv::Scalar c)
	{
		_curLabel = l;
		_labelColors[l] = c;
	}

	bool nextImg( int n=1)
	{
		if (_images.size() <= 0) return false;
		_curImageIdx +=n;

		if ( _curImageIdx >= _images.size() ) _curImageIdx = _images.size() - 1;
		
		_curImage = _images.at(_curImageIdx);
		return true;
	}

	bool prevImg(int n=1)
	{
		if (_images.size() <= 0) return false;
		_curImageIdx -=n;
		if (_curImageIdx < 0 ) _curImageIdx = 0;
		_curImage = _images.at(_curImageIdx);
		return true;
	}

	void save(const std::string & annfile )
	{
		std::ofstream annfs(annfile);

		for (auto l : _labelColors)
		{
			annfs << "l " <<l.first << " " << l.second[0] << " " << l.second[1]<<" "<< l.second[2]<< endl;
		}

		for (auto f : _pointAnnotation )
		{
			if (f.second.size() > 0)
			{
				std::cout << "p " << f.first << endl;
				for (auto p : f.second)
				{
					annfs << "p " << p._l << " " << f.first << " " << p._p.x << " " << p._p.y << " " << p._edge << endl;
				}
			}
			else
			{
				std::cout << "i " << f.first << endl;
				annfs << "i " << f.first << endl;
			}
		}
	}

	void load(const std::string & annfile)
	{
		int x, y, edge;
		char c;
		std::string fn;
		{
			std::ifstream annfs(annfile);
			int label;
			int r, g, b;
			while (annfs >> c)
			{
				if (c == 'p')
				{
					annfs >> label >> fn >> x >> y >> edge;
					addPoint(fn, cv::Point(x, y), edge, label);
				}
				else if (c == 'l')
				{
					annfs >> label >> r >> g >>b;
					addLabel( label, cv::Scalar(r,g,b) );
				}
				else if (c == 'i')
				{
					annfs >> fn;
					addImage(fn);
				}
			}
		}
		firstLabel();
	}

	void exportCsv(const std::string & opath, int w=32, int h=32)
	{
		assert(w / 2 * 2 == w);
		assert(h / 2 * 2 == h);

		ofstream fstrain(opath + "/train.txt");
		ofstream fstest(opath + "/test.txt");
		int cnt = 0;
		for (auto f : _pointAnnotation)
		{
			for (auto p : f.second)
			{
				cnt++;
			}
		}

		int curidx = 0;
		Mat dbg(cv::Size(w * 40, h* 20), CV_8UC1, Scalar(255));
		int ix = 0, iy = 0;

		for (auto f : _pointAnnotation)
		{
			if (f.second.size() > 0)
			{
				std::string fn = f.first;

				cout << "Exporting :" << fn << endl;
				cv::Mat img = cv::imread(imgbasepath + fn);
				cv::pyrDown(img, img);
				cv::pyrDown(img, img);

				Mat bgr[3];   //destination array
				split(img, bgr);//split source

				Mat o = bgr[1].clone();
				

				cv::imshow("O", o);

				for (auto p : f.second)
				{
					int xedge = w / 2;
					int yedge = h / 2;
					cv::Rect roirect(cv::Point(p._p.x - xedge, p._p.y - yedge), cv::Size(w, h));
					bool is_inside = (roirect & cv::Rect(0, 0, bgr[1].cols, bgr[1].rows)) == roirect;
					if (!is_inside)
					{
						std::cerr << "Warning! Roi not inside image" << std::endl;
						continue;
					}

					cv::Mat r = o(roirect);

					//cv::normalize(r, r, 255, 0, NORM_MINMAX);

					Mat outx, outy;
					Mat grad;
					int scale = 1;
					int delta = 0;
					int ddepth = CV_16S;

					GaussianBlur(r, r, Size(3, 3), 0, 0, BORDER_DEFAULT);
					
					
					Mat grad_x, grad_y;
					Mat abs_grad_x, abs_grad_y;
					//Scharr( src_gray, grad_x, ddepth, 1, 0, scale, delta, BORDER_DEFAULT );
					Sobel(r, grad_x, ddepth, 1, 0, 3, scale, delta, BORDER_DEFAULT);
					convertScaleAbs(grad_x, abs_grad_x);
					//Scharr( src_gray, grad_y, ddepth, 0, 1, scale, delta, BORDER_DEFAULT );
					Sobel(r, grad_y, ddepth, 0, 1, 3, scale, delta, BORDER_DEFAULT);
					convertScaleAbs(grad_y, abs_grad_y);
					addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, grad);

					cv::normalize(grad, r, 255, 0, NORM_MINMAX);


					cv::imshow("R", r);
					r.copyTo( dbg(cv::Rect(cv::Point(ix*w, iy*h), cv::Size(w, h))));

					ix++;

					if (ix * w >= dbg.cols)
					{
						ix = 0;
						iy++;
						cv::waitKey(1);
					}

					if (iy * h >= dbg.rows)
					{
						ix = 0;
						iy = 0;
					}
					cv::imshow("dbg", dbg);
					

					if (curidx++ < ((cnt * 3) / 4))
					{
						fstrain << p._l << " ";
						unsigned char *input = (unsigned char*)(r.data);
						for (int i = 0; i < r.cols; i++)
						{
							for (int j = 0; j < r.rows; j++)
							{
								fstrain << (int)r.at<unsigned char>(i, j) << " ";
							}
						}
						fstrain << std::endl;
					}
					else
					{
						fstest << p._l << " ";
						unsigned char *input = (unsigned char*)(r.data);
						for (int i = 0; i < r.cols; i++)
						{
							for (int j = 0; j < r.rows; j++)
							{
								fstest << (int)r.at<unsigned char>(i, j) << " ";
							}
						}
						fstest << std::endl;
					}
				}
			}
		}
	}
};

Annotator ann;


cv::Mat _curMat;
int edge = 20;
cv::Rect _lastRect;

std::map<int, cv::Scalar> _labelColors;

void update()
{
	string ifn = imgbasepath +"/"+ ann.curImg();
	_curMat = imread(ifn);

	for (auto i = 0; i < ann._scale; i++)
	{
		cv::pyrDown(_curMat, _curMat);
	}

	for (auto p : ann._pointAnnotation[ann._curImage])
	{
		cv::circle(_curMat, cv::Point(p._p.x, p._p.y), 3, ann._labelColors[p._l], 2);
		cv::rectangle(_curMat, cv::Point(p._p.x - p._edge, p._p.y - p._edge), cv::Point(p._p.x + p._edge, p._p.y + p._edge), ann._labelColors[p._l]);
	}
	int x = 100;
	int w = 50;
	for (auto p : ann._labelColors)
	{
		cv::circle(_curMat, cv::Point(x, 20), 5, p.second, 3);
		if (p.first == ann.curLabel())
		{
			cv::rectangle(_curMat, cv::Point(x - edge, 20 - edge), cv::Point(x + edge, 20 + edge), p.second);
		}
		x += w;
	}
	cv::imshow("CurImage", _curMat);
	cv::waitKey(1);
}

void CallBackFunc(int event, int x, int y, int flags, void* userdata)
{
	if (event == EVENT_MOUSEMOVE && flags == EVENT_FLAG_LBUTTON)
	{
		cv::Rect r = cv::Rect(cv::Point(x - edge/2, y - edge/2), cv::Point(x + edge/2, y + edge/2));
		if (((r & _lastRect).area() <= 0))
		{
			cout << "Aded pos position (" << x << ", " << y << ")" << endl;
			ann.addPoint(cv::Point(x, y),edge);
			cv::circle(_curMat, cv::Point(x, y), 3, ann.curLabelColor(), 2);
			_lastRect = r; // 
			cv::rectangle(_curMat, cv::Rect(cv::Point(x - edge, y - edge), cv::Point(x + edge, y + edge)) , ann.curLabelColor());

			cv::imshow("CurImage", _curMat);
		}
	}
	
	else if (event == EVENT_RBUTTONDOWN)
	{
		cout << "Remove pos position (" << x << ", " << y << ")" << endl;
		ann.removePoint(cv::Point(x, y), edge);
		update();
	}
	else if (event == EVENT_LBUTTONDOWN)
	{
		cout << "Aded pos position (" << x << ", " << y << ")" << endl;
		ann.addPoint(cv::Point(x, y),edge);
		cv::circle(_curMat, cv::Point(x, y), 3, ann.curLabelColor(), 2);
		cv::rectangle(_curMat, cv::Point(x - edge, y - edge), cv::Point(x + edge, y + edge), ann.curLabelColor());

		cv::imshow("CurImage", _curMat);
	}
}

const String keys =
"{help h usage ? |      | print this message          }"
"{base           |.     | path to dir                 }"
"{ann            |.     | path to annotation file     }"
"{out            |.     | path to csv extraction file }"
"{add            |.     | image-list file (create with: ls -1) }"
;

int main(int argc, char** argv)
{
	CommandLineParser parser(argc, argv, keys);
	parser.about("Anotator v1.0.0");

	if (parser.has("help"))
	{
		parser.printMessage();
		return 0;
	}

	imgbasepath = parser.get<String>("base");
	annfile = parser.get<String>("ann");
	outbasepath = parser.get<String>("out");
	std::string addimages = parser.get<String>("add");
	

	if (!parser.check())
	{
		parser.printErrors();
		return 0;
	}

	
	
	if (addimages.size() > 0)
	{
		std::string fn;
		std::ifstream infs( addimages);
		while (infs >> fn)
		{
			ann.addImage(fn);
		}
	}

	
	for (int i = 0; i < 10; i++) {
		ann.addLabel(i);
	}
	ann.firstLabel();
	

	ann.load(annfile);

	//Create a window
	namedWindow("CurImage", 1);

	//set the callback function for any mouse event
	setMouseCallback("CurImage", CallBackFunc, NULL);
	
	update();

	while(true)
	{
		// Wait until user press some key
		char k = waitKey(0);

		switch (k)
		{
		case 'q':
			exit(0);

		case '1':
		{
			ann.prevImg();
			update();
			continue;
		}
		break;

		case '2':
		{
			ann.nextImg();
			update();
			continue;
		}
		break;

		case '3':
		{
			ann.prevImg(10);
			update();
			continue;
		}
		break;

		case '4':
		{
			ann.nextImg(10);
			update();
			continue;
		}
		break;

		case 's':
		{
			ann.save(annfile);
		}
		break;

		case 'c':
		{
			ann.nextLabel();
			update();
		}
		break;

		case '+':
		{
			edge++;
			update();
		}
		break;

		case '-':
		{
			edge--;
			if (edge <= 2) edge = 2;
			update();
		}
		case 'z':
		{
			ann._scale = ann._scale >= 2 ?  0 : ann._scale + 1;
			update();
		}
		break;

		case 'x':
		{
			ann.exportCsv(outbasepath,32,32);
		}
		break;

		case 'a':
		{
			ann.addLabel(ann._labelColors.size());
			update();
		}
		break;

		default:
			break;
		}
	}

	return 0;

}