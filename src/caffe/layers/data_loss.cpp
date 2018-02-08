
#include <vector>

#include "caffe/layer.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/layers/data_loss.hpp"
#include "caffe/layers/st_layer.hpp"
#include "caffe/layers/conv_layer.hpp"
#include "caffe/layers/power_layer.hpp"
#include "caffe/layers/eltwise_layer.hpp"

namespace caffe {

template <typename Dtype>
void DataLossLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  LossLayer<Dtype>::LayerSetUp(bottom, top);  
  
  if(bottom.size() == 3) {
    // Set up stn layer and eltwise layer to compute elementwise difference

    
    stn_top_vec_.clear();
    stn_top_vec_.push_back(&stn_output_);

    stn_bottom_vec_.clear();
    stn_bottom_vec_.push_back(bottom[1]);
    stn_bottom_vec_.push_back(bottom[2]); 
    LayerParameter stn_param;
    stn_layer_.reset(new SpatialTransformerLayer<Dtype>(stn_param));   
    stn_layer_->SetUp(stn_bottom_vec_,stn_top_vec_);

    
    
    diff_top_vec_.clear();
    diff_top_vec_.push_back(&diff_);
  
    diff_bottom_vec_.clear();
    diff_bottom_vec_.push_back(bottom[0]);
    diff_bottom_vec_.push_back(stn_top_vec_[0]);
  
    LayerParameter diff_param;
    diff_param.mutable_eltwise_param()->add_coeff(1.);
    diff_param.mutable_eltwise_param()->add_coeff(-1.);
    diff_param.mutable_eltwise_param()->set_operation(EltwiseParameter_EltwiseOp_SUM);
    diff_layer_.reset(new EltwiseLayer<Dtype>(diff_param));
    diff_layer_->SetUp(diff_bottom_vec_, diff_top_vec_);
   
  } else {
      LOG(FATAL) << "DataLossLayer needs three input blobs.";
  }
  
  
    // Set up power layer to compute elementwise square
   
    square_top_vec_.clear();
    square_top_vec_.push_back(&square_output_);
    LayerParameter square_param;
    square_param.mutable_power_param()->set_power(Dtype(2));
    square_layer_.reset(new PowerLayer<Dtype>(square_param));
    square_layer_->SetUp(diff_top_vec_, square_top_vec_);
  
    // Set up convolutional layer to sum all channels
    
    sum_top_vec_.clear();
    sum_top_vec_.push_back(&sum_output_);
    LayerParameter sum_param;  
    sum_param.mutable_convolution_param()->set_num_output(1);
    sum_param.mutable_convolution_param()->add_kernel_size(1);
    sum_param.mutable_convolution_param()->mutable_weight_filler()->set_type("constant");
    
    sum_param.mutable_convolution_param()->mutable_weight_filler()->set_value(Dtype(1));
    
    sum_layer_.reset(new ConvolutionLayer<Dtype>(sum_param));
   
    sum_layer_->SetUp(square_top_vec_, sum_top_vec_);
  
    // Set up power layer to compute elementwise sqrt
    sqrt_top_vec_.clear();
    sqrt_top_vec_.push_back(&sqrt_output_);
    LayerParameter sqrt_param;
    sqrt_param.mutable_power_param()->set_power(0.5);
    sqrt_param.mutable_power_param()->set_shift(this->layer_param_.data_loss_param().epsilon());
    sqrt_layer_.reset(new PowerLayer<Dtype>(sqrt_param));
    sqrt_layer_->SetUp(sum_top_vec_, sqrt_top_vec_);
   
}

template <typename Dtype>
void DataLossLayer<Dtype>::Reshape(
  const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {

 vector<int> loss_shape(0);  // Loss layers output a scalar; 0 axes.
  top[0]->Reshape(loss_shape);
  
  stn_layer_->Reshape(stn_bottom_vec_,stn_top_vec_);
  diff_layer_->Reshape(diff_bottom_vec_, diff_top_vec_);
  
  
  sign_.Reshape(bottom[0]->num(), bottom[0]->channels(),
                bottom[0]->height(), bottom[0]->width());
 
  mask_.Reshape(bottom[0]->num(), bottom[0]->channels(),
                bottom[0]->height(), bottom[0]->width());

  plateau_l2_.ReshapeLike(sum_output_);
  
  
  square_layer_->Reshape(diff_top_vec_, square_top_vec_);
  sum_layer_->Reshape(square_top_vec_, sum_top_vec_);
  sqrt_layer_->Reshape(sum_top_vec_, sqrt_top_vec_);    
  caffe_set(sign_.count()/sign_.channels(), Dtype(1), sign_.mutable_cpu_data());
  
  
}

template <typename Dtype>
void DataLossLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  NOT_IMPLEMENTED;
}

template <typename Dtype>
void DataLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  NOT_IMPLEMENTED;
}

#ifdef CPU_ONLY
STUB_GPU(DataLossLayer);
#endif

INSTANTIATE_CLASS(DataLossLayer);
REGISTER_LAYER_CLASS(DataLoss);

}  // namespace caffe
